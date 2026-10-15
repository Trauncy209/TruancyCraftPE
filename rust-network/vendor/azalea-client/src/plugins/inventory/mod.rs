pub mod equipment_effects;
pub mod shared_slots;

use azalea_chat::FormattedText;
use azalea_core::tick::GameTick;
use azalea_entity::{PlayerAbilities, inventory::Inventory as Inv};
use azalea_inventory::operations::ClickOperation;
pub use azalea_inventory::*;
use azalea_protocol::packets::game::{
    s_container_click::{HashedStack, ServerboundContainerClick},
    s_container_close::ServerboundContainerClose,
    s_set_carried_item::ServerboundSetCarriedItem,
};
use azalea_registry::builtin::MenuKind;
use azalea_world::{InstanceContainer, InstanceName};
use bevy_app::{App, Plugin};
use bevy_ecs::prelude::*;
use indexmap::IndexMap;
use tracing::{error, warn};

use crate::{
    Client,
    inventory::equipment_effects::{collect_equipment_changes, handle_equipment_changes},
    packet::game::SendGamePacketEvent,
};

// TODO: when this is removed, remove the Inv alias above (which just exists to
// avoid conflicting with this pub deprecated type)
#[doc(hidden)]
#[deprecated = "moved to `azalea_entity::inventory::Inventory`."]
pub type Inventory = azalea_entity::inventory::Inventory;

pub struct InventoryPlugin;
impl Plugin for InventoryPlugin {
    fn build(&self, app: &mut App) {
        app.add_systems(
            GameTick,
            (
                ensure_has_sent_carried_item.after(super::mining::handle_mining_queued),
                collect_equipment_changes
                    .after(super::interact::handle_start_use_item_queued)
                    .before(azalea_physics::ai_step),
            ),
        )
        .add_observer(handle_client_side_close_container_trigger)
        .add_observer(handle_menu_opened_trigger)
        .add_observer(handle_container_close_event)
        .add_observer(handle_set_container_content_trigger)
        .add_observer(handle_container_click_event)
        // number keys are checked on tick but scrolling can happen outside of ticks, therefore
        // this is fine
        .add_observer(handle_set_selected_hotbar_slot_event)
        .add_observer(handle_equipment_changes);
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq, SystemSet)]
pub struct InventorySystems;

impl Client {
    /// Return the menu that is currently open, or the player's inventory if no
    /// menu is open.
    pub fn menu(&self) -> Menu {
        self.query_self::<&Inv, _>(|inv| inv.menu().clone())
    }

    /// Returns the index of the hotbar slot that's currently selected.
    ///
    /// If you want to access the actual held item, you can get the current menu
    /// with [`Client::menu`] and then get the slot index by offsetting from
    /// the start of [`azalea_inventory::Menu::hotbar_slots_range`].
    ///
    /// You can use [`Self::set_selected_hotbar_slot`] to change it.
    pub fn selected_hotbar_slot(&self) -> u8 {
        self.query_self::<&Inv, _>(|inv| inv.selected_hotbar_slot)
    }

    /// Update the selected hotbar slot index.
    ///
    /// This will run next `Update`, so you might want to call
    /// `bot.wait_updates(1)` after calling this if you're using `azalea`.
    ///
    /// # Panics
    ///
    /// This will panic if `new_hotbar_slot_index` is not in the range 0..=8.
    pub fn set_selected_hotbar_slot(&self, new_hotbar_slot_index: u8) {
        assert!(
            new_hotbar_slot_index < 9,
            "Hotbar slot index must be in the range 0..=8"
        );

        let mut ecs = self.ecs.lock();
        ecs.trigger(SetSelectedHotbarSlotEvent {
            entity: self.entity,
            slot: new_hotbar_slot_index,
        });
    }
}

/// A Bevy trigger that's fired when our client should show a new screen (like a
/// chest or crafting table).
///
/// To watch for the menu being closed, you could use
/// [`ClientsideCloseContainerEvent`]. To close it manually, use
/// [`CloseContainerEvent`].
#[derive(Clone, Debug, EntityEvent)]
pub struct MenuOpenedEvent {
    pub entity: Entity,
    pub window_id: i32,
    pub menu_type: MenuKind,
    pub title: FormattedText,
}
#[derive(Component,Clone,Default)]
pub struct PlayerInventoryState {pub state_id:u32,pub carried:ItemStack}
fn handle_menu_opened_trigger(event: On<MenuOpenedEvent>,mut commands:Commands,mut query: Query<(&mut Inv,Option<&mut PlayerInventoryState>)>) {
    let (mut inventory,player_state) = query.get_mut(event.entity).unwrap();
    if inventory.id==0 {let saved=PlayerInventoryState{state_id:inventory.state_id,carried:inventory.carried.clone()};if let Some(mut state)=player_state{*state=saved;}else{commands.entity(event.entity).insert(saved);}}
    inventory.state_id=0;inventory.carried=ItemStack::Empty;
    inventory.id = event.window_id;
    inventory.container_menu = Some(Menu::from_kind(event.menu_type));
    shared_slots::player_to_active(&mut inventory);
    inventory.container_menu_title = Some(event.title.clone());
}

/// Tell the server that we want to close a container.
///
/// Note that this is also sent when the client closes its own inventory, even
/// though there is no packet for opening its inventory.
#[derive(EntityEvent)]
pub struct CloseContainerEvent {
    pub entity: Entity,
    /// The ID of the container to close. 0 for the player's inventory.
    ///
    /// If this is not the same as the currently open inventory, nothing will
    /// happen.
    pub id: i32,
}
fn handle_container_close_event(
    close_container: On<CloseContainerEvent>,
    mut commands: Commands,
    query: Query<(Entity, &Inv)>,
) {
    let (entity, inventory) = query.get(close_container.entity).unwrap();
    if close_container.id != inventory.id {
        warn!(
            "Tried to close container with ID {}, but the current container ID is {}",
            close_container.id, inventory.id
        );
        return;
    }

    commands.trigger(SendGamePacketEvent::new(
        entity,
        ServerboundContainerClose {
            container_id: inventory.id,
        },
    ));
    commands.trigger(ClientsideCloseContainerEvent {
        entity: close_container.entity,
    });
}

/// A Bevy event that's fired when our client closed a container.
///
/// This can also be triggered directly to close a container silently without
/// sending any packets to the server. You probably don't want that though, and
/// should instead use [`CloseContainerEvent`].
///
/// If you want to watch for a container being opened, you should use
/// [`MenuOpenedEvent`].
#[derive(Clone, EntityEvent)]
pub struct ClientsideCloseContainerEvent {
    pub entity: Entity,
}
pub fn handle_client_side_close_container_trigger(
    event: On<ClientsideCloseContainerEvent>,
    mut query: Query<(&mut Inv,Option<&PlayerInventoryState>)>,
) {
    let (mut inventory,player_state) = query.get_mut(event.entity).unwrap();
    let had_container=inventory.container_menu.is_some();

    // All live mutations mirror the shared slots. Keep the final copy as a
    // consistency boundary for external users of the public Inventory fields.
    shared_slots::active_to_player(&mut inventory);
    inventory.container_menu=None;

    inventory.id = 0;
    inventory.container_menu_title = None;
    if had_container {inventory.state_id=player_state.map(|state|state.state_id).unwrap_or(0);inventory.carried=player_state.map(|state|state.carried.clone()).unwrap_or(ItemStack::Empty);}
}

#[derive(Debug, EntityEvent)]
pub struct ContainerClickEvent {
    pub entity: Entity,
    pub window_id: i32,
    pub operation: ClickOperation,
}
pub fn handle_container_click_event(
    container_click: On<ContainerClickEvent>,
    mut commands: Commands,
    mut query: Query<(Entity, &mut Inv, Option<&PlayerAbilities>, &InstanceName)>,
    instance_container: Res<InstanceContainer>,
) {
    let (entity, mut inventory, player_abilities, instance_name) =
        query.get_mut(container_click.entity).unwrap();
    if inventory.id != container_click.window_id {
        error!(
            "Tried to click container with ID {}, but the current container ID is {}. Click packet won't be sent.",
            container_click.window_id, inventory.id
        );
        return;
    }

    let Some(instance) = instance_container.get(instance_name) else {
        return;
    };

    let old_slots = inventory.menu().slots();
    inventory.simulate_click(
        &container_click.operation,
        player_abilities.unwrap_or(&PlayerAbilities::default()),
    );
    shared_slots::active_to_player(&mut inventory);
    let new_slots = inventory.menu().slots();

    let registry_holder = &instance.read().registries;

    // see which slots changed after clicking and put them in the map the server
    // uses this to check if we desynced
    let mut changed_slots: IndexMap<u16, HashedStack> = IndexMap::new();
    for (slot_index, old_slot) in old_slots.iter().enumerate() {
        let new_slot = &new_slots[slot_index];
        if old_slot != new_slot {
            changed_slots.insert(
                slot_index as u16,
                HashedStack::from_item_stack(new_slot, registry_holder),
            );
        }
    }

    commands.trigger(SendGamePacketEvent::new(
        entity,
        ServerboundContainerClick {
            container_id: container_click.window_id,
            state_id: inventory.state_id,
            slot_num: container_click
                .operation
                .slot_num()
                .map(|n| n as i16)
                .unwrap_or(-999),
            button_num: container_click.operation.button_num(),
            click_type: container_click.operation.click_type(),
            changed_slots,
            carried_item: HashedStack::from_item_stack(&inventory.carried, registry_holder),
        },
    ));
}

/// Sent from the server when the contents of a container are replaced.
///
/// Usually triggered by the `ContainerSetContent` packet.
#[derive(EntityEvent)]
pub struct SetContainerContentEvent {
    pub entity: Entity,
    pub slots: Vec<ItemStack>,
    pub container_id: i32,
    pub state_id:u32,
    pub carried_item:ItemStack,
}
pub fn handle_set_container_content_trigger(
    set_container_content: On<SetContainerContentEvent>,
    mut query: Query<&mut Inv>,
) {
    let mut inventory = query.get_mut(set_container_content.entity).unwrap();

    if set_container_content.container_id != inventory.id {
        warn!(
            "Got SetContainerContentEvent for container with ID {}, but the current container ID is {}",
            set_container_content.container_id, inventory.id
        );
        return;
    }

    inventory.state_id=set_container_content.state_id;inventory.carried=set_container_content.carried_item.clone();
    for (i, slot) in set_container_content.slots.iter().enumerate() {
        shared_slots::set_active_slot(&mut inventory,i,slot);
    }
}

/// An ECS message to switch our hand to a different hotbar slot.
///
/// This is equivalent to using the scroll wheel or number keys in Minecraft.
#[derive(EntityEvent)]
pub struct SetSelectedHotbarSlotEvent {
    pub entity: Entity,
    /// The hotbar slot to select. This should be in the range 0..=8.
    pub slot: u8,
}
pub fn handle_set_selected_hotbar_slot_event(
    set_selected_hotbar_slot: On<SetSelectedHotbarSlotEvent>,
    mut query: Query<&mut Inv>,
) {
    let mut inventory = query.get_mut(set_selected_hotbar_slot.entity).unwrap();
    inventory.selected_hotbar_slot = set_selected_hotbar_slot.slot;
}

/// The item slot that the server thinks we have selected.
///
/// See [`ensure_has_sent_carried_item`].
#[derive(Component)]
pub struct LastSentSelectedHotbarSlot {
    pub slot: u8,
}
/// A system that makes sure that [`LastSentSelectedHotbarSlot`] is in sync with
/// [`Inv::selected_hotbar_slot`].
///
/// This is necessary to make sure that [`ServerboundSetCarriedItem`] is sent in
/// the right order, since it's not allowed to happen outside of a tick.
pub fn ensure_has_sent_carried_item(
    mut commands: Commands,
    query: Query<(Entity, &Inv, Option<&LastSentSelectedHotbarSlot>)>,
) {
    for (entity, inventory, last_sent) in query.iter() {
        if let Some(last_sent) = last_sent {
            if last_sent.slot == inventory.selected_hotbar_slot {
                continue;
            }

            commands.trigger(SendGamePacketEvent::new(
                entity,
                ServerboundSetCarriedItem {
                    slot: inventory.selected_hotbar_slot as u16,
                },
            ));
        }

        commands.entity(entity).insert(LastSentSelectedHotbarSlot {
            slot: inventory.selected_hotbar_slot,
        });
    }
}

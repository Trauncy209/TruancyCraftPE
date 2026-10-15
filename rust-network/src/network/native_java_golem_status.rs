//! Authoritative golem entity-status animations, independent of damage/death effects.
use std::collections::BTreeMap;
#[derive(Default, Clone, Copy, Debug, PartialEq)]
pub struct Timers { pub attack: u16, pub flower: u16 }
#[derive(Default)]
pub struct GolemStatuses { states: BTreeMap<i32, Timers> }
impl GolemStatuses {
    pub fn spawn(&mut self, id: i32, iron_golem: bool) {
        self.states.remove(&id);
        if iron_golem { self.states.insert(id, Timers::default()); }
    }
    pub fn event(&mut self, id: i32, event: u8) {
        let Some(timers) = self.states.get_mut(&id) else { return };
        match event { 4 => timers.attack = 10, 11 => timers.flower = 400,
            34 => timers.flower = 0, _ => {} }
    }
    pub fn tick(&mut self) {
        for timers in self.states.values_mut() {
            timers.attack = timers.attack.saturating_sub(1);
            timers.flower = timers.flower.saturating_sub(1);
        }
    }
    pub fn snapshot(&self, id: i32) -> Timers { self.states.get(&id).copied().unwrap_or_default() }
    pub fn remove(&mut self, id: i32) { self.states.remove(&id); }
    pub fn clear(&mut self) { self.states.clear(); }
}
#[cfg(test)] mod tests {
    use super::*;
    #[test] fn actual_events_drive_separate_snapshot_timers_and_expire() {
        let mut status=GolemStatuses::default();status.spawn(7,true);
        status.event(7,4);status.event(7,11);
        assert_eq!(status.snapshot(7),Timers{attack:10,flower:400});
        status.tick();assert_eq!(status.snapshot(7),Timers{attack:9,flower:399});
        for _ in 0..9 {status.tick();}
        assert_eq!(status.snapshot(7),Timers{attack:0,flower:390});
        status.event(7,34);assert_eq!(status.snapshot(7).flower,0);
        status.event(7,11);for _ in 0..401 {status.tick();}
        assert_eq!(status.snapshot(7),Timers::default());
    }
    #[test] fn unrelated_kinds_removal_world_reset_and_reused_ids_cannot_animate() {
        let mut status=GolemStatuses::default();status.spawn(8,false);
        for event in [4,11,34] {status.event(8,event);status.event(999,event);}
        assert_eq!(status.snapshot(8),Timers::default());
        status.spawn(8,true);status.event(8,4);status.spawn(8,false);
        assert_eq!(status.snapshot(8),Timers::default());
        status.spawn(9,true);status.event(9,11);status.remove(9);
        assert_eq!(status.snapshot(9),Timers::default());
        status.spawn(10,true);status.event(10,4);status.clear();
        assert_eq!(status.snapshot(10),Timers::default());
    }
}

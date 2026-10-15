import com.mojang.minecraftpe.LegacyWorldMigration;
import java.nio.file.*;
import java.util.Arrays;
public class LegacyWorldMigrationTest {
 static void check(boolean b){if(!b)throw new AssertionError();}
 public static void main(String[] args)throws Exception{
  Path base=Files.createTempDirectory("tcpe-world-test"),src=base.resolve("old"),dst=base.resolve("new/minecraftWorlds");
  Path old=src.resolve("world");Files.createDirectories(old.resolve("db"));byte[] bytes={0,1,-1,65};Files.write(old.resolve("level.dat"),bytes);Files.write(old.resolve("db/chunk"),bytes);
  Files.createDirectories(dst.resolve("world"));Files.write(dst.resolve("world/level.dat"),new byte[]{9});
  LegacyWorldMigration.Result r=LegacyWorldMigration.copyWorlds(src.toFile(),dst.toFile());check(r.imported==1&&r.failed==0);
  check(Arrays.equals(bytes,Files.readAllBytes(dst.resolve("world_legacy_1/db/chunk"))));check(Files.readAllBytes(dst.resolve("world/level.dat"))[0]==9);check(Arrays.equals(bytes,Files.readAllBytes(old.resolve("level.dat"))));
  r=LegacyWorldMigration.copyWorlds(src.toFile(),dst.toFile());check(r.imported==0&&r.skipped==1);
  Path invalid=src.resolve("invalid");Files.createDirectories(invalid);Files.write(invalid.resolve("level.dat"),bytes);Path secret=base.resolve("outside");Files.write(secret,bytes);Files.createSymbolicLink(invalid.resolve("escape"),secret);
  r=LegacyWorldMigration.copyWorlds(src.toFile(),dst.toFile());check(r.failed==1&&r.imported==0&&!Files.exists(dst.resolve("invalid")));check(Arrays.equals(bytes,Files.readAllBytes(secret)));check(Files.exists(invalid.resolve("level.dat")));
  try(java.util.stream.Stream<Path> staged=Files.list(dst.getParent().resolve(".tcpe-world-import"))){check(staged.count()==0);}
  System.out.println("PASS: exact copy, original preservation, collision protection, idempotence, outside-symlink rejection and partial-copy cleanup");
 }
}

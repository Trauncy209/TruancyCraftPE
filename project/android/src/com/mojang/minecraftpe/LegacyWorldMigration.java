package com.mojang.minecraftpe;

import java.io.*;
import java.util.HashSet;
import java.util.UUID;

// Copies legacy worlds into app-owned storage. Never moves/deletes a source
// world or overwrites an existing destination world. Incomplete copies stay
// outside the native world's directory and are never exposed as playable.
public final class LegacyWorldMigration {
    private static final String MARKER = ".tcpe-legacy-source";
    public static final class Result { public int imported, skipped, failed; }
    public static Result copyWorlds(File source, File destination) throws IOException {
        Result result = new Result();
        source = source.getCanonicalFile(); destination = destination.getCanonicalFile();
        if (source.equals(destination) || !source.isDirectory()) return result;
        if (!destination.isDirectory() && !destination.mkdirs()) throw new IOException("Cannot create world folder");
        File staging = new File(destination.getParentFile(), ".tcpe-world-import");
        if (!staging.isDirectory() && !staging.mkdirs()) throw new IOException("Cannot stage world import");
        File[] worlds = source.listFiles(); if (worlds == null) throw new IOException("Cannot read old worlds");
        for (File world : worlds) {
            if (!world.isDirectory() || !new File(world, "level.dat").isFile()) continue;
            String origin = world.getCanonicalPath();
            if (!origin.startsWith(source.getPath() + File.separator)) { result.failed++; continue; }
            if (alreadyImported(destination, origin)) { result.skipped++; continue; }
            File complete = new File(destination, world.getName());
            for (int suffix = 1; complete.exists(); suffix++) complete = new File(destination, world.getName() + "_legacy_" + suffix);
            File temporary = new File(staging, UUID.randomUUID().toString());
            try {
                copyTree(world, temporary, origin, 0, new HashSet<String>());
                writeText(new File(temporary, MARKER), origin);
                if (!temporary.renameTo(complete)) throw new IOException("Cannot publish imported world");
                result.imported++;
            } catch (IOException e) { result.failed++; removeCreatedTree(temporary); }
        }
        return result;
    }
    private static boolean alreadyImported(File destination, String origin) throws IOException {
        File[] entries = destination.listFiles(); if (entries == null) return false;
        for (File entry : entries) {
            File marker = new File(entry, MARKER);
            if (marker.isFile() && marker.length() < 8192 && origin.equals(readText(marker))) return true;
        }
        return false;
    }
    private static void copyTree(File source, File target, String root, int depth, HashSet<String> visited) throws IOException {
        String canonical = source.getCanonicalPath();
        if (depth > 64 || (!canonical.equals(root) && !canonical.startsWith(root + File.separator)) || !visited.add(canonical)) throw new IOException("Invalid world path");
        if (source.isDirectory()) {
            if (!target.mkdir()) throw new IOException("Cannot copy world folder");
            File[] children = source.listFiles(); if (children == null) throw new IOException("Cannot read world folder");
            for (File child : children) copyTree(child, new File(target, child.getName()), root, depth + 1, visited);
        } else {
            try (InputStream in = new FileInputStream(source); OutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[65536]; int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
            }
            target.setLastModified(source.lastModified());
        }
    }
    private static String readText(File path) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (InputStream in = new FileInputStream(path)) { byte[] b = new byte[1024]; int n; while ((n=in.read(b))!=-1) out.write(b,0,n); }
        return out.toString("UTF-8");
    }
    private static void writeText(File path, String text) throws IOException { try (OutputStream out = new FileOutputStream(path)) { out.write(text.getBytes("UTF-8")); } }
    private static void removeCreatedTree(File path) {
        if (path.isDirectory()) { File[] files = path.listFiles(); if (files != null) for (File file : files) removeCreatedTree(file); }
        path.delete();
    }
    private LegacyWorldMigration() {}
}

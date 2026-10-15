package com.mojang.minecraftpe;
// Bounded native audio bridge; all operations run on the Activity UI thread.
final class TcpeAudio {
    private final android.app.Activity activity;
    private final java.util.LinkedHashMap<android.media.MediaPlayer,Request> longVoices=new java.util.LinkedHashMap<android.media.MediaPlayer,Request>();
    private android.media.SoundPool pool;
    private int rainVoiceLimit=48;
    private java.io.File cacheDirectory;
    private boolean paused;
    private final java.util.HashSet<android.media.MediaPlayer> pausedLong=new java.util.HashSet<android.media.MediaPlayer>();
    private final java.util.HashSet<Integer> pausedShort=new java.util.HashSet<Integer>();
    private final java.util.LinkedHashMap<String,Integer> samples=new java.util.LinkedHashMap<String,Integer>(128,.75f,true);
    private final java.util.HashSet<Integer> ready=new java.util.HashSet<Integer>();
    private final java.util.HashMap<Integer,java.util.ArrayList<Request>> pending=new java.util.HashMap<Integer,java.util.ArrayList<Request>>();
    private final java.util.LinkedHashMap<Integer,Request> voices=new java.util.LinkedHashMap<Integer,Request>();
    private static final class Request {String event;int category;float gain,pitch;Request(String n,int c,float g,float p){event=n;category=c;gain=g;pitch=p;}}
    TcpeAudio(android.app.Activity activity){this.activity=activity;}
    private java.io.File soundDirectory(){
        if(cacheDirectory!=null)return cacheDirectory;
        String version="original-format-v2";
        try{java.security.MessageDigest hash=java.security.MessageDigest.getInstance("SHA-256");java.io.InputStream input=activity.getAssets().open("sounds/java/manifest.json");try{byte[] bytes=new byte[4096];int total=0,n;while((n=input.read(bytes))!=-1){total+=n;if(total>65536)throw new java.io.IOException("Oversized sound manifest");hash.update(bytes,0,n);}}finally{input.close();}StringBuilder key=new StringBuilder();byte[] digest=hash.digest();for(int i=0;i<12;++i)key.append(String.format(java.util.Locale.ROOT,"%02x",digest[i]&255));version=key.toString();}catch(java.io.IOException|java.security.NoSuchAlgorithmException ignored){}
        cacheDirectory=new java.io.File(activity.getCacheDir(),"tcpe-sounds-"+version);cacheDirectory.mkdirs();return cacheDirectory;
    }
    private boolean file(String p){return p!=null&&p.matches("sounds/java/[0-9a-f]{24}\\.wav");}
    private boolean name(String n){return n!=null&&(n.isEmpty()||n.length()<=256&&n.matches("[a-z0-9_.-]+:[a-z0-9_./-]+"));}
    private void init(){if(pool!=null)return;try{pool=new android.media.SoundPool(64,android.media.AudioManager.STREAM_MUSIC,0);rainVoiceLimit=48;}catch(RuntimeException unavailable){pool=new android.media.SoundPool(24,android.media.AudioManager.STREAM_MUSIC,0);rainVoiceLimit=12;}pool.setOnLoadCompleteListener(new android.media.SoundPool.OnLoadCompleteListener(){public void onLoadComplete(android.media.SoundPool p,int id,int status){if(p!=pool)return;java.util.ArrayList<Request> list=pending.remove(id);if(status!=0)return;ready.add(id);if(list!=null)for(Request request:list)start(id,request);}});}
    private static boolean rain(Request request){return request.event.equals("minecraft:weather.rain")||request.event.equals("minecraft:weather.rain.above");}
    private void start(int sample,Request request){
        final boolean ambience=rain(request);
        if(ambience){
            int count=0;Integer oldest=null;
            for(java.util.Map.Entry<Integer,Request> entry:voices.entrySet())if(rain(entry.getValue())){++count;if(oldest==null)oldest=entry.getKey();}
            // Same bounded ambience policy as OpenAL: retain at least twelve voices for
            // effects, recycle oldest rain tails without reducing event cadence.
            if(count>=rainVoiceLimit&&oldest!=null){pool.stop(oldest);voices.remove(oldest);pausedShort.remove(oldest);}
        }
        // SoundPool may reclaim lower-priority rain for gameplay, never the
        // reverse. Legacy audio has its own pool; a 24-stream fallback remains available.
        int stream=pool.play(sample,request.gain,request.gain,ambience?0:1,0,request.pitch);
        if(stream==0)return;
        voices.remove(stream);voices.put(stream,request);
        if(paused){pool.pause(stream);pausedShort.add(stream);}
        while(voices.size()>128){Integer old=voices.keySet().iterator().next();pool.stop(old);pausedShort.remove(old);voices.remove(old);}
    }
    private void stop(int category,String event){java.util.Iterator<java.util.Map.Entry<android.media.MediaPlayer,Request>> longs=longVoices.entrySet().iterator();while(longs.hasNext()){java.util.Map.Entry<android.media.MediaPlayer,Request> entry=longs.next();Request r=entry.getValue();if(r.category>=0&&(category==255||r.category==category)&&(event.isEmpty()||event.equals(r.event))){pausedLong.remove(entry.getKey());entry.getKey().release();longs.remove();}}if(pool==null)return;java.util.Iterator<java.util.Map.Entry<Integer,Request>> it=voices.entrySet().iterator();while(it.hasNext()){java.util.Map.Entry<Integer,Request> entry=it.next();Request r=entry.getValue();if(r.category>=0&&(category==255||r.category==category)&&(event.isEmpty()||event.equals(r.event))){pool.stop(entry.getKey());pausedShort.remove(entry.getKey());it.remove();}}for(java.util.ArrayList<Request> list:pending.values()){java.util.Iterator<Request> requests=list.iterator();while(requests.hasNext()){Request r=requests.next();if(r.category>=0&&(category==255||r.category==category)&&(event.isEmpty()||event.equals(r.event)))requests.remove();}}}
    private void play(String path,Request request)throws java.io.IOException{init();Integer sample=samples.get(path);if(sample==null){java.io.File directory=soundDirectory();directory.mkdirs();java.io.File cached=new java.io.File(directory,path.substring(path.lastIndexOf('/')+1));if(!cached.exists()){java.io.InputStream input=activity.getAssets().open(path);java.io.FileOutputStream output=new java.io.FileOutputStream(cached);try{byte[] b=new byte[16384];int n;while((n=input.read(b))!=-1)output.write(b,0,n);}finally{input.close();output.close();}}if(cached.length()>1000000){playLong(cached,request);return;}sample=pool.load(cached.getAbsolutePath(),1);if(sample==0)return;samples.put(path,sample);while(samples.size()>128){String old=samples.keySet().iterator().next();int id=samples.remove(old);pool.unload(id);ready.remove(id);pending.remove(id);}}
        if(ready.contains(sample))start(sample,request);else{java.util.ArrayList<Request> list=pending.get(sample);if(list==null){list=new java.util.ArrayList<Request>();pending.put(sample,list);}if(list.size()<32)list.add(request);}}
    public void command(final String command,final float gain,final float pitch){if(command==null)return;activity.runOnUiThread(new Runnable(){public void run(){if(activity.isFinishing())return;try{String[] fields=command.split("\\|",4);if(fields[0].equals("stop")&&fields.length==3){int c=Integer.parseInt(fields[1]);if((c==255||c>=0&&c<=9)&&name(fields[2]))stop(c,fields[2]);return;}if(fields[0].equals("reset")&&fields.length==1){destroy();return;}if(fields[0].equals("register")&&fields.length==3&&file(fields[1])&&fields[2].length()<=24*1024*1024){byte[] wav=android.util.Base64.decode(fields[2],android.util.Base64.DEFAULT);if(wav.length<44||wav.length>16*1024*1024+44)return;java.io.File directory=soundDirectory();directory.mkdirs();java.io.File target=new java.io.File(directory,fields[1].substring(fields[1].lastIndexOf('/')+1));java.io.FileOutputStream out=new java.io.FileOutputStream(target);try{out.write(wav);}finally{out.close();}return;}
        String path=command,event=command;int category=-1;if(fields[0].equals("play")&&fields.length==4){category=Integer.parseInt(fields[1]);event=fields[2];path=fields[3];if(category < -1||category>9||!name(event))return;}if(!file(path)||Float.isNaN(gain)||Float.isInfinite(gain)||Float.isNaN(pitch)||Float.isInfinite(pitch)||gain<=0||pitch<=0)return;play(path,new Request(event,category,Math.max(0,Math.min(1,gain)),Math.max(.5f,Math.min(2,pitch))));}catch(java.io.IOException|IllegalArgumentException e){/* Invalid or unavailable server sound remains silent. */}}});}
    private void playLong(java.io.File file,final Request request)throws java.io.IOException{
        while(longVoices.size()>=4){android.media.MediaPlayer old=longVoices.keySet().iterator().next();longVoices.remove(old);pausedLong.remove(old);old.release();}
        final android.media.MediaPlayer player=new android.media.MediaPlayer();longVoices.put(player,request);
        try{player.setDataSource(file.getAbsolutePath());player.setVolume(request.gain,request.gain);
        player.setOnPreparedListener(new android.media.MediaPlayer.OnPreparedListener(){public void onPrepared(android.media.MediaPlayer p){if(!longVoices.containsKey(p))return;if(android.os.Build.VERSION.SDK_INT>=23)p.setPlaybackParams(new android.media.PlaybackParams().setSpeed(request.pitch).setPitch(request.pitch));if(paused)pausedLong.add(p);else p.start();}});
        player.setOnCompletionListener(new android.media.MediaPlayer.OnCompletionListener(){public void onCompletion(android.media.MediaPlayer p){pausedLong.remove(p);longVoices.remove(p);p.release();}});
        player.setOnErrorListener(new android.media.MediaPlayer.OnErrorListener(){public boolean onError(android.media.MediaPlayer p,int what,int extra){pausedLong.remove(p);longVoices.remove(p);p.release();return true;}});player.prepareAsync();
        }catch(java.io.IOException|RuntimeException e){longVoices.remove(player);player.release();throw e;}
    }
    public void pause(){paused=true;if(pool!=null)pool.autoPause();for(android.media.MediaPlayer p:longVoices.keySet())try{if(p.isPlaying()){pausedLong.add(p);p.pause();}}catch(IllegalStateException ignored){}}
    public void resume(){paused=false;if(pool!=null){pool.autoResume();for(Integer stream:pausedShort)pool.resume(stream);}pausedShort.clear();for(android.media.MediaPlayer p:pausedLong)if(longVoices.containsKey(p))try{p.start();}catch(IllegalStateException ignored){}pausedLong.clear();}
    public void destroy(){for(android.media.MediaPlayer p:longVoices.keySet())p.release();longVoices.clear();pausedLong.clear();pausedShort.clear();if(pool!=null){pool.release();pool=null;}samples.clear();ready.clear();pending.clear();voices.clear();}
}

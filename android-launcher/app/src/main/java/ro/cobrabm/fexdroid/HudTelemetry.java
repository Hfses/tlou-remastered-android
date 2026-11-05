package ro.cobrabm.fexdroid;

import android.app.ActivityManager;
import android.content.Context;
import android.system.Os;
import android.system.OsConstants;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.Locale;

/**
 * Low-frequency /proc telemetry for the actual TLOU child process.
 * Values reflect process CPU time and RSS, not fabricated GPU/VRAM figures.
 * Missing or restricted procfs entries are omitted from the HUD.
 */
final class HudTelemetry {
    private final long ticksPerSecond;
    private long previousTicks=-1L, previousAt;
    private double cpuPercent=Double.NaN;
    private double memoryGiB=Double.NaN, deviceGiB=Double.NaN;
    private volatile String gpuName="";

    HudTelemetry(Context context) {
        long hz=100;
        try { hz=Os.sysconf(OsConstants._SC_CLK_TCK); }catch(Throwable ignored){}
        ticksPerSecond=hz>0?hz:100;
        try {
            ActivityManager activity=(ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo info=new ActivityManager.MemoryInfo();
            if(activity!=null) {activity.getMemoryInfo(info);deviceGiB=info.totalMem/(1024.0*1024.0*1024.0);}
        }catch(Throwable ignored){}
        // EGL context creation can block: never do it in the 500ms HUD/UI poll.
        Thread detector=new Thread(()->{
            try {
                String renderer=BuiltinDrivers.renderer();
                if(renderer!=null && renderer.toLowerCase(Locale.ROOT).contains("adreno")) {
                    java.util.regex.Matcher m=java.util.regex.Pattern.compile(
                        "(?i)adreno[^0-9]*([0-9]{3})").matcher(renderer);
                    gpuName=m.find()?"Adreno "+m.group(1):"Adreno";
                }
            }catch(Throwable ignored){}
        },"tlou-hud-gpu-name");
        detector.setDaemon(true);detector.start();
    }

    private static long processTicks(int pid) throws Exception {
        try(BufferedReader reader=new BufferedReader(new FileReader("/proc/"+pid+"/stat"))) {
            String line=reader.readLine();
            if(line==null)throw new IllegalStateException("Sem estatísticas de CPU");
            int end=line.lastIndexOf(')');
            if(end<0||end+2>=line.length())throw new IllegalArgumentException("Formato de /proc/stat");
            String[] fields=line.substring(end+2).trim().split("\\s+");
            return Long.parseLong(fields[11])+Long.parseLong(fields[12]); // utime+stime
        }
    }

    void sample(int pid,long nowNanos) {
        if(pid<=0)return;
        try {
            long ticks=processTicks(pid);
            if(previousTicks>=0 && nowNanos>previousAt && ticks>=previousTicks)
                cpuPercent=(ticks-previousTicks)*100.0*1_000_000_000.0/
                    (ticksPerSecond*(nowNanos-previousAt)); // percent of one CPU core
            previousTicks=ticks;previousAt=nowNanos;
        }catch(Exception e){cpuPercent=Double.NaN;}
        try(BufferedReader reader=new BufferedReader(new FileReader("/proc/"+pid+"/status"))) {
            String line;
            while((line=reader.readLine())!=null)if(line.startsWith("VmRSS:")) {
                String digits=line.substring(6).replaceAll("[^0-9]","");
                if(!digits.isEmpty())memoryGiB=Long.parseLong(digits)/(1024.0*1024.0);
                break;
            }
        }catch(Exception e){memoryGiB=Double.NaN;}
    }
    double cpuPercent(){return cpuPercent;}
    double ramGiB(){return memoryGiB;}
    double totalRamGiB(){return deviceGiB;}
    String gpuName(){return gpuName;}
    void reset(){previousTicks=-1L;previousAt=0;cpuPercent=Double.NaN;}
}

package ro.cobrabm.fexdroid;

import android.content.Context;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;

/**
 * Opt-in KGSL max-clock mode for rooted Adreno devices.
 * Android applications cannot force GPU frequency without elevated permission.
 * The session restores the original min_freq on exit; a root watchdog also
 * restores it if the Android process is terminated unexpectedly.
 */
final class GpuTurboSession {
    private String devfreq;
    private String oldMin;
    private boolean active;

    private static String runRoot(String script) throws Exception {
        Process command=new ProcessBuilder("su","-c",script).redirectErrorStream(true).start();
        if(!command.waitFor(7,TimeUnit.SECONDS)) {
            command.destroyForcibly();
            throw new IllegalStateException("Tempo esgotado ao solicitar acesso root");
        }
        StringBuilder output=new StringBuilder();
        try(BufferedReader input=new BufferedReader(new InputStreamReader(
            command.getInputStream(),StandardCharsets.UTF_8))) {
            String line;
            while((line=input.readLine())!=null && output.length()<4096)output.append(line).append('\n');
        }
        if(command.exitValue()!=0)throw new IllegalStateException(
            "O dispositivo não autorizou o clock máximo da GPU (root). "+output);
        return output.toString().trim();
    }

    synchronized boolean start(Context context) {
        if(active || !PortSettings.prefs(context).getBoolean("gpu_turbo",false))return active;
        try {
            // All values originate from fixed sysfs paths, never user input.
            String set="for d in /sys/class/kgsl/kgsl-3d0/devfreq /sys/class/devfreq/soc:qcom,kgsl-3d0; do "+
                "if [ -r \"$d/min_freq\" ] && [ -r \"$d/max_freq\" ]; then "+
                "old=$(cat \"$d/min_freq\"); maximum=$(cat \"$d/max_freq\"); "+
                "case \"$old:$maximum\" in *[!0-9:]*|:*) exit 3;; esac; "+
                "if [ \"$maximum\" -lt 1 ]; then exit 4; fi; "+
                "printf '%s' \"$maximum\" > \"$d/min_freq\" || exit 5; "+
                "printf '%s|%s\\n' \"$d\" \"$old\"; exit 0; fi; done; exit 6";
            String response=runRoot(set);
            int separator=response.lastIndexOf('|');
            if(separator<0)throw new IllegalStateException("Resposta root inesperada");
            String found=response.substring(0,separator).trim();
            String original=response.substring(separator+1).trim();
            if(!found.startsWith("/sys/class/") || !original.matches("[0-9]+"))
                throw new IllegalStateException("Clock original inválido");
            devfreq=found;oldMin=original;active=true;
            // A root-owned detached monitor prevents a persistent GPU max-min
            // clock after an app crash/force-stop, even if Java finally cannot run.
            int pid=android.os.Process.myPid();
            String watcher="(while kill -0 "+pid+" 2>/dev/null; do sleep 2; done; "+
                "printf '%s' '"+original+"' > '"+found+"/min_freq') >/dev/null 2>&1 </dev/null &";
            try { runRoot(watcher); }
            catch(Exception e){RunLog.write(context,"Aviso: watchdog GPU não iniciado: "+e);}
            RunLog.write(context,"Turbo GPU: clock mínimo elevado ao máximo, restauração automática ativada");
            return true;
        } catch(Exception e) {
            RunLog.write(context,"Turbo GPU não ativado: "+e.getMessage());
            // Do not claim clock was locked when root or KGSL is unavailable.
            return false;
        }
    }

    synchronized void stop(Context context) {
        if(!active)return;
        try {
            String path=devfreq, restore=oldMin;
            // Values were validated from the fixed KGSL path on start().
            if(path!=null && path.startsWith("/sys/class/") && restore!=null &&
                restore.matches("[0-9]+"))runRoot("printf '%s' '"+restore+"' > '"+path+"/min_freq'");
            RunLog.write(context,"Turbo GPU: clock original restaurado");
        } catch(Exception e) {
            RunLog.write(context,"Turbo GPU: falha na restauração direta; watchdog segue ativo: "+e);
        } finally {
            active=false; devfreq=null; oldMin=null;
        }
    }
    synchronized boolean isActive(){return active;}
}

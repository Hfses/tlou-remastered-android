package ro.cobrabm.fexdroid;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/** Loads only the real TLOU ARM64/FEX executable, never a renamed GOW3 game. */
public final class GameActivity extends Activity {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final AtomicBoolean closing=new AtomicBoolean();
    private SurfaceView surface;
    private TouchControllerOverlay touch;
    private PadBridge pad;
    private AudioBridge audio;
    private Process game;
    private File logFile;
    private TextView status;
    private String prepared;
    private volatile boolean started;

    private static FrameLayout.LayoutParams pos(int w,int h,int gravity) {
        return new FrameLayout.LayoutParams(w,h,gravity);
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        immersive();
        prepared=getIntent().getStringExtra("prepared_path");
        if (prepared==null) { finish(); return; }
        File gamePath=new File(prepared);
        if (!new File(gamePath,"boot-linked.bin").isFile() ||
            !new File(gamePath,"content.bin").isFile()) { finish(); return; }
        File stateDir=new File(getFilesDir(),"tlou");
        if (!stateDir.exists()) stateDir.mkdirs();
        File input=new File(stateDir,"controller.state");
        pad=new PadBridge(input);
        audio=new AudioBridge(new File(stateDir,"audio.fifo"));
        logFile=new File(stateDir,"runtime.log");

        FrameLayout layout=new FrameLayout(this);
        layout.setBackgroundColor(0xff000000);
        surface=new SurfaceView(this);
        layout.addView(surface,pos(-1,-1,Gravity.CENTER));
        touch=new TouchControllerOverlay(this,pad,false);
        layout.addView(touch,pos(-1,-1,Gravity.CENTER));
        LinearLayout bar=new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setBackgroundColor(0x99000000);
        status=new TextView(this);
        status.setTextColor(0xffeeeeee);
        status.setTextSize(12);
        status.setText("TLOU • Bionic ARM64");
        status.setGravity(Gravity.CENTER_VERTICAL);
        status.setPadding(16,6,16,6);
        bar.addView(status,new LinearLayout.LayoutParams(0,-2,1));
        Button log=new Button(this);
        log.setText("LOG");
        log.setAllCaps(false);
        log.setOnClickListener(v->showLog());
        bar.addView(log,new LinearLayout.LayoutParams(-2,-2));
        Button exit=new Button(this);
        exit.setText("SAIR");
        exit.setAllCaps(false);
        exit.setOnClickListener(v->stopAndExit());
        bar.addView(exit,new LinearLayout.LayoutParams(-2,-2));
        layout.addView(bar,pos(-1,-2,Gravity.TOP));
        setContentView(layout);

        try {
            String bridge=Gow3Present.start(surface.getHolder(),
                    new File(stateDir,"present.sock").getAbsolutePath(),false);
            if (!bridge.startsWith("bridge running")) throw new IllegalStateException(bridge);
            audio.start();
            started=true;
        } catch (Throwable err) {
            error("Não foi possível iniciar AHardwareBuffer: "+err);
            return;
        }
        Thread starter=new Thread(this::launch,"tlou-fex-launcher");
        starter.start();
        handler.postDelayed(this::refreshFrames,1200);
    }

    private void immersive() {
        if (android.os.Build.VERSION.SDK_INT>=30) {
            final WindowInsetsController controller=getWindow().getInsetsController();
            if(controller!=null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    private void update(String message) {
        handler.post(()->{
            if (status!=null && !closing.get()) status.setText(message);
        });
    }

    private void error(String message) {
        handler.post(()->new AlertDialog.Builder(this).setTitle("TLOU — erro de inicialização")
            .setMessage(message).setPositiveButton("Fechar",(d,w)->stopAndExit()).show());
    }

    private void refreshFrames() {
        if (!started || closing.get()) return;
        try {
            long frames=Gow3Present.framesIfLoaded();
            if(game!=null && game.isAlive()) status.setText("TLOU • frames apresentados: "+frames);
        } catch (Throwable ignored) {}
        handler.postDelayed(this::refreshFrames,1500);
    }

    private File installBinary() throws Exception {
        File root=new File(getFilesDir(),"tlou/runtime");
        if (!root.exists() && !root.mkdirs()) throw new IllegalStateException("Diretório de runtime indisponível");
        File dst=new File(root,"gow3-probe");
        File tmp=new File(root,"gow3-probe.tmp");
        try(InputStream in=getAssets().open("runtime/gow3-probe");
            FileOutputStream out=new FileOutputStream(tmp)) {
            byte[] buf=new byte[128*1024];
            int n;
            while((n=in.read(buf))!=-1) out.write(buf,0,n);
        }
        if(tmp.length()<8192) throw new IllegalStateException("Runtime TLOU inválido no APK");
        if (!tmp.setExecutable(true,true)) throw new IllegalStateException("Falhou chmod do loader Bionic");
        if (dst.exists()&&!dst.delete()) throw new IllegalStateException("Falhou substituir runtime");
        if(!tmp.renameTo(dst)) throw new IllegalStateException("Falhou instalar loader");
        return dst;
    }

    private void launch() {
        try {
            File bin=installBinary();
            File base=new File(prepared);
            File app0=new File(base,"tlou-view");
            File state=new File(getFilesDir(),"tlou");
            File user=new File(state,"user");
            if (!user.exists() && !user.mkdirs()) throw new IllegalStateException("Falha no usuário PS4");
            if (!app0.isDirectory()) throw new IllegalStateException("tlou-view ausente, rode o preparador TLOU");
            List<String> args=new ArrayList<>();
            args.add(bin.getAbsolutePath());
            args.add(new File(base,"boot-linked.bin").getAbsolutePath());
            args.add("--low-image");
            args.add("--no-game-hooks");
            args.add("--content-profile");
            args.add(new File(base,"content.bin").getAbsolutePath());
            args.add("--app0");
            args.add(app0.getAbsolutePath());
            args.add("--user");
            args.add(user.getAbsolutePath());
            File patches=new File(base,"patches.bin");
            if (patches.isFile() && patches.length()>0) {
                args.add("--patches");
                args.add(patches.getAbsolutePath());
            }
            ProcessBuilder builder=new ProcessBuilder(args);
            Map<String,String> env=builder.environment();
            env.put("LD_LIBRARY_PATH",getApplicationInfo().nativeLibraryDir);
            env.put("GOW3_ANDROID_PORT","1");
            env.put("GOW3_ANDROID_INPUT","1");
            env.put("GOW3_PAD_FILE",new File(state,"controller.state").getAbsolutePath());
            env.put("GOW3_AUDIO_FIFO",new File(state,"audio.fifo").getAbsolutePath());
            env.put("GOW3_PRESENT_SOCKET",new File(state,"present.sock").getAbsolutePath());
            env.put("GOW3_SCREEN_W",String.valueOf(getResources().getDisplayMetrics().widthPixels));
            env.put("GOW3_SCREEN_H",String.valueOf(getResources().getDisplayMetrics().heightPixels));
            env.put("GOW3_DISPLAY_HZ",String.valueOf(Math.round(getWindowManager().getDefaultDisplay().getRefreshRate())));
            env.put("GOW3_GPU_USER_DIR",state.getAbsolutePath());
            env.put("GOW3_VSYNC","1");
            String driver=getSharedPreferences("tlou",MODE_PRIVATE).getString("driver_path", "");
            if(!driver.isEmpty()) {
                File custom=new File(driver);
                if(!custom.isFile()) throw new IllegalStateException("Driver selecionado ausente; selecione outro driver no menu");
                DriverArchive.validateLibrary(custom);
                env.put("GOW3_VULKAN_DRIVER",custom.getAbsolutePath());
                env.put("LD_LIBRARY_PATH",custom.getParent()+":"+getApplicationInfo().nativeLibraryDir);
            }
            builder.directory(app0);
            builder.redirectErrorStream(true);
            builder.redirectOutput(ProcessBuilder.Redirect.to(logFile));
            update("Iniciando TLOU x86-64 pelo FEX...");
            game=builder.start();
            final int result=game.waitFor();
            if (!closing.get()) {
                String message="Runtime finalizado (código "+result+"). Veja o log.";
                update(message);
                error(message+"\n"+tailLog());
            }
        } catch (Exception err) {
            if(!closing.get()) error("Falha ao iniciar runtime TLOU: "+err+"\n"+tailLog());
        }
    }

    private String tailLog() {
        try {
            if(logFile==null || !logFile.exists()) return "Log ainda não criado";
            List<String> lines=Files.readAllLines(logFile.toPath(),StandardCharsets.UTF_8);
            int start=Math.max(0,lines.size()-40);
            return String.join("\n",lines.subList(start,lines.size()));
        } catch(Exception err) { return "Falha ao ler log: "+err; }
    }

    private void showLog() {
        new AlertDialog.Builder(this).setTitle("TLOU — log nativo")
                .setMessage(tailLog()).setPositiveButton("Fechar",null).show();
    }

    private void stopAndExit() {
        if (!closing.compareAndSet(false,true)) return;
        if (game!=null) game.destroy();
        try { if(touch!=null) touch.release(); } catch(Exception ignored) {}
        try { if(audio!=null) audio.stop(); } catch(Exception ignored) {}
        try { if(started) Gow3Present.stop(); } catch(Exception ignored) {}
        finish();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        if(pad!=null && pad.key(e)) return true;
        return super.dispatchKeyEvent(e);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if(pad!=null && pad.motion(e)) return true;
        return super.dispatchGenericMotionEvent(e);
    }

    @Override protected void onPause() {
        super.onPause();
        if(audio!=null) audio.pause();
    }

    @Override protected void onResume() {
        super.onResume();
        immersive();
        if(audio!=null) audio.resume();
    }

    @Override protected void onDestroy() {
        stopAndExit();
        super.onDestroy();
    }
}

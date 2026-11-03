package ro.cobrabm.fexdroid;

import android.content.Context;
import android.content.SharedPreferences;
import org.json.JSONObject;
import java.io.File;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.Map;

final class PortSettings {
    static final String[] EFFECTS={"ssao","motion_blur","depth_of_field","bloom","sunlight_shadows","subsurface"};
    static SharedPreferences prefs(Context context) { return context.getSharedPreferences("tlou",Context.MODE_PRIVATE); }
    static File patchOptions(Context context) throws Exception {
        SharedPreferences p=prefs(context);
        JSONObject options=new JSONObject(); options.put("resolution",p.getString("resolution","original"));
        for(String key:EFFECTS) options.put(key,p.getBoolean(key,false));
        File file=new File(context.getFilesDir(),"patch-options.json");
        Files.write(file.toPath(),options.toString().getBytes(StandardCharsets.UTF_8));
        return file;
    }
    static void environment(Context context,Map<String,String> env) {
        SharedPreferences p=prefs(context);
        String resolution=p.getString("resolution","original");
        env.put("GOW3_VSYNC",p.getBoolean("vsync",true)?"1":"0");
        env.put("GOW3_FPS_LIMIT",p.getString("fps_limit","60"));
        env.put("GOW3_FRAMES_AHEAD",p.getString("frames_ahead","1"));
        env.put("GOW3_READBACKS",resolution.equals("original")?p.getString("readback","1"):"2");
        env.put("GOW3_ASYNC_SHADERS",p.getBoolean("async_shaders",true)?"1":"0");
        // Keep stale/deferred image reads off; changing their semantics needs device validation.
        env.put("GOW3_DEFERRED_READBACK","0"); env.put("GOW3_STALE_READBACK","0");
    }
    static String summary(Context context) {
        SharedPreferences p=prefs(context); String r=p.getString("resolution","original");
        return (r.equals("original")?"Resolução original":r+"p")+"  ·  "+p.getString("fps_limit","60")+" FPS  ·  "+
            (p.getBoolean("vsync",true)?"VSync ligado":"VSync desligado");
    }
}

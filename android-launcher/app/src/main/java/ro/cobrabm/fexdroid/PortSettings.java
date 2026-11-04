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
        env.put("TLOU_EXPECT_SCENE_SIZE",resolution.equals("540")?"960x540":
            resolution.equals("720")?"1280x720":resolution.equals("360")?"640x360":"1920x1080");
        env.put("TLOU_MAPPED_FAULT_READBACKS",p.getBoolean("mapped_fault_readbacks",false)?"1":"0");
        env.put("TLOU_MAPPED_READBACKS",p.getBoolean("mapped_readbacks",true)?"1":"0");
        // Profiling is separate from optimization and off during FPS comparisons.
        // Remove disabled keys: legacy renderer sites test presence, not their value.
        for(String key:new String[]{"GOW3_GPU_PROFILE","GOW3_FRAME_STATS","GOW3_PERF_DIAG"}) {
            if(p.getBoolean("gpu_profile",false)) env.put(key,"1"); else env.remove(key);
        }
        env.put("GOW3_VSYNC",p.getBoolean("vsync",true)?"1":"0");
        env.put("GOW3_FPS_LIMIT",p.getString("fps_limit","60"));
        env.put("GOW3_FRAMES_AHEAD",p.getString("frames_ahead","1"));
        // Resolution must not silently switch synchronization strategies. 540p/360p
        // can be slower and glitchy with precise readbacks; keep it an A/B choice.
        String readback=p.getString("readback","1");
        env.put("GOW3_READBACKS",readback.equals("2")?"2":"1");
        env.put("GOW3_ASYNC_SHADERS",p.getBoolean("async_shaders",true)?"1":"0");
        // Independent opt-in knobs already implemented in the TLOU renderer.
        // All new experiments remain OFF by default.
        env.put("GOW3_PREP_POOL",p.getBoolean("prep_pool",false)?"1":"0");
        env.put("GOW3_CPU_TRIMS",p.getBoolean("cpu_trims",false)?"1":"0");
        env.put("GOW3_PREFETCH_PIPELINES",p.getBoolean("prefetch_pipelines",false)?"1":"0");
        env.put("GOW3_PARALLEL_PIPELINES",p.getBoolean("parallel_pipelines",false)?"1":"0");
        String workers=p.getString("prep_workers","auto");
        if(workers.equals("0")||workers.equals("2")||workers.equals("4")||workers.equals("6"))
            env.put("GOW3_PREP_WORKERS",workers);
        else env.remove("GOW3_PREP_WORKERS"); // original hardware-based selection
        // Keep stale/deferred image reads off; changing their semantics needs device validation.
        env.put("GOW3_DEFERRED_READBACK","0"); env.put("GOW3_STALE_READBACK","0");
    }
    static String experimentReport(Context context,Map<String,String> env) {
        StringBuilder result=new StringBuilder("TLOU A/B: resolution=")
            .append(prefs(context).getString("resolution","original"));
        for(String key:new String[]{"TLOU_MAPPED_FAULT_READBACKS","TLOU_MAPPED_READBACKS","GOW3_GPU_PROFILE","GOW3_READBACKS","GOW3_FRAMES_AHEAD",
                "GOW3_ASYNC_SHADERS","GOW3_PREP_POOL","GOW3_CPU_TRIMS",
                "GOW3_PREFETCH_PIPELINES","GOW3_PARALLEL_PIPELINES",
                "GOW3_PREP_WORKERS"})
            result.append(" ").append(key).append("=")
                .append(env.containsKey(key)?env.get(key):"auto");
        return result.toString();
    }
    static String summary(Context context) {
        SharedPreferences p=prefs(context); String r=p.getString("resolution","original");
        String limit=p.getString("fps_limit","60");
        return (r.equals("original")?"Resolução original":r+"p")+"  ·  "+(limit.equals("0")?"Sem limite":limit+" FPS")+"  ·  "+
            (p.getBoolean("vsync",true)?"VSync ligado":"VSync desligado");
    }
}

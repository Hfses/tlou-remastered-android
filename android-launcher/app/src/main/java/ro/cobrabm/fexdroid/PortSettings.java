package ro.cobrabm.fexdroid;

import android.content.Context;
import android.os.Build;
import java.util.Locale;
import android.content.SharedPreferences;
import org.json.JSONObject;
import java.io.File;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/** Log 32 launch profile. Experimental A/B switches are NOT user preferences. */
final class PortSettings {
    static final String[] EFFECTS={"ssao","motion_blur","depth_of_field","bloom","sunlight_shadows","subsurface"};
    static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences("tlou",Context.MODE_PRIVATE);
    }

    // Existing player-selected game, driver, patch and resolution preferences survive upgrades.
    // Old experiment preferences cannot silently reactivate discarded modes.
    static void retireTestPreferences(Context context) {
        SharedPreferences.Editor edit=prefs(context).edit();
        for(String key:new String[]{"memory_walk_once","ring_binding_cursor","draw_counter_trim",
            "compute_perm_memo","host_copy_batch","srt_hot_readbacks","fetch_no_block",
            "land_on_guest","static_min_512","stack_mapped_readback","inline_single_copy",
            "split_cs64","mapped_fault_readbacks","mapped_readbacks","gpu_profile",
            "prep_pool","cpu_trims","prefetch_pipelines","parallel_pipelines",
            "prep_workers","frames_ahead","descriptor_bulk","readback","sotc_precise_narrow",
            "compute_wait_fast","relaxed_skinning_fp"}) edit.remove(key);
        String limit=prefs(context).getString("fps_limit","60");
        if(!"30".equals(limit) && !"60".equals(limit))edit.putString("fps_limit","60");
        edit.apply();
    }

    static File patchOptions(Context context) throws Exception {
        SharedPreferences p=prefs(context);
        JSONObject options=new JSONObject();
        options.put("resolution",p.getString("resolution","original"));
        // CUSA00552 01.00 on Xiaomi 23049PCD8G (Snapdragon 7+ Gen 2)
        // hit a game job-system deadlock after 806 s. The forced job-sleep
        // patch replaces two conditional branches with unconditional waits;
        // preserve the game's original worker-wake decisions on this SOC.
        // Other device profiles (including Adreno 830) remain unchanged.
        options.put("job_sleep",!conservativeJobWorkers());
        for(String key:EFFECTS) options.put(key,p.getBoolean(key,false));
        File file=new File(context.getFilesDir(),"patch-options.json");
        Files.write(file.toPath(),options.toString().getBytes(StandardCharsets.UTF_8));
        return file;
    }

    static boolean conservativeJobWorkers() {
        final String model=Build.MODEL==null?"":Build.MODEL.toUpperCase(Locale.ROOT);
        final String soc=Build.VERSION.SDK_INT>=31 && Build.SOC_MODEL!=null
            ?Build.SOC_MODEL.toUpperCase(Locale.ROOT):"";
        // SM7475 = Snapdragon 7+ Gen 2; 23049PCD8G is the exact phone in
        // TLOU-Android-Logs(5).zip. Do not assume other Adreno 7xx are affected.
        return soc.contains("SM7475") || model.equals("23049PCD8G");
    }

    static void environment(Context context,Map<String,String> env) {
        SharedPreferences p=prefs(context);
        String resolution=p.getString("resolution","original");
        env.put("TLOU_EXPECT_SCENE_SIZE",resolution.equals("540")?"960x540":
            resolution.equals("720")?"1280x720":resolution.equals("360")?"640x360":"1920x1080");

        // Exact flags observed in launch.log of TLOU-Android-Logs (32).zip.
        // Its preceding launch-previous.log used different v0.31 trial flags:
        // the more recent actual run had the three below OFF.
        env.put("TLOU_MEMORY_WALK_ONCE","1");
        env.put("TLOU_RING_BINDING_CURSOR","1");
        env.put("TLOU_DRAW_COUNTER_TRIM","0");
        env.put("GOW3_TLOU_COMPUTE_MEMO","0");
        env.put("GOW3_TLOU_HOST_COPY_BATCH","0");
        env.put("TLOU_SRT_HOT_READBACKS","1");
        env.put("TLOU_SRT_DEP_BATCH","0");
        env.put("GOW3_FETCH_NO_BLOCK","1");
        env.put("GOW3_LAND_ON_GUEST","1");
        env.put("GOW3_STATIC_MIN_BYTES","512");
        env.put("TLOU_STACK_MAPPED_READBACK","1");
        env.put("TLOU_INLINE_SINGLE_COPY","1");
        env.put("TLOU_SPLIT_CS64","1");
        env.put("TLOU_MAPPED_FAULT_READBACKS","1");
        env.put("TLOU_MAPPED_READBACKS","1");
        // The native Android default is ON unless explicitly disabled.
        env.put("TLOU_CPU_PHASE_SAMPLES","0");
        // Leave profiling truly disabled. Some native switches check presence,
        // so setting "0" instead of removing is incorrect.
        for(String name:new String[]{"GOW3_GPU_PROFILE","GOW3_FRAME_STATS","GOW3_PERF_DIAG"})
            env.remove(name);

        env.put("GOW3_READBACKS","1"); // Precise=2 caused artifacts; never resurrect.
        env.put("GOW3_EOP_READBACK","1"); // Selective readbacks, not Precise/Readback 2.
        // Temporary bounded coverage audit for CUSA00552 01.00. This only logs
        // at most 40 distinct candidate compute bindings; no shader dumps or waits.
        env.put("TLOU_PHYSICS_COVERAGE","1");
        env.put("GOW3_ASYNC_SHADERS","1");
        env.put("GOW3_FRAMES_AHEAD","0");
        env.put("TLOU_DESCRIPTOR_BULK","0");
        env.put("GOW3_PREP_POOL","1");
        env.put("GOW3_CPU_TRIMS","1");
        env.put("GOW3_PREFETCH_PIPELINES","1");
        env.put("GOW3_PARALLEL_PIPELINES","1");
        env.put("GOW3_PREP_WORKERS","0");
        env.put("GOW3_DEFERRED_READBACK","0");
        env.put("GOW3_STALE_READBACK","0");

        // Only presentation controls remain user-adjustable.
        env.put("GOW3_VSYNC",p.getBoolean("vsync",true)?"1":"0");
        env.put("GOW3_FPS_LIMIT",fpsLimit(context));
    }

    static String experimentReport(Context context,Map<String,String> env) {
        StringBuilder result=new StringBuilder("TLOU launch settings: resolution=")
            .append(prefs(context).getString("resolution","original"));
        result.append("; renderer=Vulkan; CPU=FEX ARM64");
        return result.toString();
    }

    static String fpsLimit(Context context) {
        return "30".equals(prefs(context).getString("fps_limit","60"))?"30":"60";
    }
    static String summary(Context context) {
        SharedPreferences p=prefs(context);
        String r=p.getString("resolution","original"), limit=fpsLimit(context);
        String res=r.equals("original")?"Original":r+"p";
        return res+"  ·  "+(limit+" FPS")+"  ·  "+
            (p.getBoolean("vsync",true)?"VSync ON":"VSync OFF");
    }
}

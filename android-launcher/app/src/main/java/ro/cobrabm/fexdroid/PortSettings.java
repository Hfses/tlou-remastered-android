package ro.cobrabm.fexdroid;

import android.content.Context;
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
        edit.apply();
    }

    static File patchOptions(Context context) throws Exception {
        SharedPreferences p=prefs(context);
        JSONObject options=new JSONObject();
        options.put("resolution",p.getString("resolution","original"));
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
        env.put("GOW3_FPS_LIMIT",p.getString("fps_limit","60"));
    }

    static String experimentReport(Context context,Map<String,String> env) {
        StringBuilder result=new StringBuilder("TLOU launch settings: resolution=")
            .append(prefs(context).getString("resolution","original"));
        result.append("; renderer=Vulkan; CPU=FEX ARM64");
        return result.toString();
    }

    static String summary(Context context) {
        SharedPreferences p=prefs(context);
        String r=p.getString("resolution","original"), limit=p.getString("fps_limit","60");
        String res=r.equals("original")?"Original":r+"p";
        return res+"  ·  "+(limit.equals("0")?"FPS livre":limit+" FPS")+"  ·  "+
            (p.getBoolean("vsync",true)?"VSync ON":"VSync OFF");
    }
}

package ro.cobrabm.fexdroid;

import android.content.Context;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/** App-only translations. No game/guest language override is ever sent. */
final class TlouLanguage {
    static final String[] CODES={"pt","en","es","fr","de","it","tr","pl","ru","hi"};
    static final String[] NAMES={"Português (Brasil)","English","Español","Français",
        "Deutsch","Italiano","Türkçe","Polski","Русский","हिन्दी"};
    private static Map<String,String[]> dictionary;
    private TlouLanguage() {}

    static int index(Context context) {
        String saved=PortSettings.prefs(context).getString("ui_language","pt");
        for(int i=0;i<CODES.length;i++)if(CODES[i].equals(saved))return i;
        return 0;
    }

    private static synchronized void ensureLoaded(Context context) {
        if(dictionary!=null)return;
        Map<String,String[]> loaded=new HashMap<>();
        try(BufferedReader r=new BufferedReader(new InputStreamReader(
            context.getAssets().open("ui-translations.tsv"),StandardCharsets.UTF_8))) {
            String line;
            while((line=r.readLine())!=null) {
                if(line.isEmpty() || line.charAt(0)=='#')continue;
                String[] fields=line.split("\\t",-1);
                if(fields.length!=CODES.length || fields[0].isEmpty())continue;
                loaded.put(fields[0],fields);
            }
        } catch(Exception ignored) {
            // Never break gameplay or settings merely because translation
            // assets are missing in a developer checkout.
        }
        dictionary=loaded;
    }

    static String t(Context context,String original) {
        if(original==null)return "";
        final int lang=index(context);
        if(lang==0)return original;
        ensureLoaded(context);
        String[] values=dictionary.get(original);
        if(values==null || values[lang].isEmpty())return original;
        return values[lang];
    }
}

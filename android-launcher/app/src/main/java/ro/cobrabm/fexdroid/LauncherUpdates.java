package ro.cobrabm.fexdroid;

import android.content.Context;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** User-requested update check. Opens the official release page for manual
 * installation; no silent APK install and no alternative signing key. */
final class LauncherUpdates {
    static final String REPO="erickmendes22/tlou-remastered-android";
    private static final String API="https://api.github.com/repos/"+REPO+"/releases/latest";
    static final class Version {
        final String tag,releaseUrl;
        final boolean newer;
        Version(String tag,String releaseUrl,boolean newer){
            this.tag=tag;this.releaseUrl=releaseUrl;this.newer=newer;
        }
    }
    static long[] segments(String raw) {
        Matcher m=Pattern.compile("^v?(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?(?:[-+].*)?$",
                                  Pattern.CASE_INSENSITIVE).matcher(raw);
        if(!m.matches())return new long[]{-1,0,0};
        long[] result=new long[3];
        try {for(int i=0;i<3;i++)result[i]=m.group(i+1)==null?0:Long.parseLong(m.group(i+1));}
        catch(NumberFormatException e){return new long[]{-1,0,0};}
        return result;
    }
    static boolean isNewer(String current,String candidate) {
        long[] a=segments(current),b=segments(candidate);
        if(a[0]<0 || b[0]<0)return false;
        for(int i=0;i<3;i++){
            if(b[i]>a[i])return true;
            if(b[i]<a[i])return false;
        }
        return false;
    }
    static Version check(Context context) throws Exception {
        HttpURLConnection conn=(HttpURLConnection)new URL(API).openConnection();
        conn.setConnectTimeout(9000);conn.setReadTimeout(9000);
        conn.setRequestProperty("Accept","application/vnd.github+json");
        conn.setRequestProperty("User-Agent","TLOU-Android-Launcher");
        try {
            if(conn.getResponseCode()!=200)throw new java.io.IOException(
                "GitHub response "+conn.getResponseCode());
            StringBuilder body=new StringBuilder();
            try(BufferedReader in=new BufferedReader(new InputStreamReader(
                conn.getInputStream(),StandardCharsets.UTF_8))){
                String line;
                while((line=in.readLine())!=null){
                    body.append(line);
                    if(body.length()>131072)throw new java.io.IOException("Release response too large");
                }
            }
            JSONObject release=new JSONObject(body.toString());
            String tag=release.getString("tag_name"),link=release.getString("html_url");
            // Never let a remote API response redirect an install/update button
            // off the official project's GitHub release tree.
            String expected="https://github.com/"+REPO+"/releases/";
            if(!link.startsWith(expected) || !link.matches(
                "https://github\\.com/erickmendes22/tlou-remastered-android/releases/[A-Za-z0-9._/%-]+"))
                throw new java.io.IOException("Invalid release URL");
            String installed=context.getPackageManager()
                .getPackageInfo(context.getPackageName(),0).versionName;
            return new Version(tag,link,isNewer(installed,tag));
        } finally {conn.disconnect();}
    }
}

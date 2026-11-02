package ro.cobrabm.fexdroid;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.Objects;

/**
 * The Last of Us Remastered (CUSA00552) Android launcher.
 * Only accepts prepared TLOU data, never silently falls back to a GOW3 executable.
 */
public final class MainActivity extends Activity {
    private static final int PICK_PREPARED = 501;
    private static final int PICK_DRIVER = 502;
    private EditText path;
    private TextView status;
    private final int dark = 0xff10151a;
    private final int foreground = 0xffe4e9ed;

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setStatusBarColor(dark);
        getWindow().setNavigationBarColor(dark);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(dark);
        LinearLayout root = new LinearLayout(this);
        root.setPadding(26, 40, 26, 26);
        root.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(root);

        TextView title = text("THE LAST OF US REMASTERED\nANDROID • BIONIC / FEX", 23);
        title.setTextColor(0xffd9e2d7);
        root.addView(title);
        TextView subtitle = text("Port experimental CUSA00552. Não acompanha jogo ou arquivos protegidos. Use um dump próprio preparado para este port.", 15);
        root.addView(subtitle);

        root.addView(text("Pasta preparada do TLOU", 17));
        path = new EditText(this);
        path.setSingleLine(false);
        path.setTextColor(foreground);
        path.setHintTextColor(0xff82909c);
        path.setHint("/storage/emulated/0/TLOU-Prepared");
        path.setText(getPreferences(0).getString("prepared_path", ""));
        root.addView(path);

        root.addView(button("Escolher pasta preparada", () -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            pick.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(pick, PICK_PREPARED);
        }));

        root.addView(button("Permitir acesso aos arquivos", () -> {
            if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                try { startActivity(intent); }
                catch (Exception ex) { startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)); }
            } else {
                alert("Arquivos", "O acesso aos arquivos já está permitido ou não é necessário nesta versão.");
            }
        }));

        root.addView(button("Importar driver Vulkan (.so) opcional", () -> {
            Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            picker.setType("*/*");
            picker.addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(picker, PICK_DRIVER);
        }));

        root.addView(button("INICIAR JOGO", this::launch));
        root.addView(button("Instruções de preparação", () ->
                alert("Preparação TLOU", "O jogo precisa de boot-linked.bin, content.bin e tlou-view, gerados com android/tlou_bionic.py do repositório. Selecione a pasta de saída. O APK não substitui esta etapa nem contém jogos.")));

        status = text("", 14);
        root.addView(status);
        setContentView(scroll);
        refresh();
    }

    private TextView text(String message, int size) {
        TextView tv = new TextView(this);
        tv.setText(message);
        tv.setTextColor(foreground);
        tv.setTextSize(size);
        tv.setPadding(0, 10, 0, 10);
        return tv;
    }

    private Button button(String name, Runnable action) {
        Button b = new Button(this);
        b.setAllCaps(false);
        b.setText(name);
        b.setOnClickListener(v -> action.run());
        return b;
    }

    private void alert(String title, String message) {
        new AlertDialog.Builder(this).setTitle(title).setMessage(message)
                .setPositiveButton("OK", null).show();
    }

    private String folder() { return path.getText().toString().trim(); }

    private void refresh() {
        if (status == null) return;
        File selected = new File(folder());
        boolean prepared = new File(selected, "boot-linked.bin").isFile() &&
                           new File(selected, "content.bin").isFile() &&
                           new File(selected, "tlou-view").isDirectory();
        status.setText(prepared ? "Imagem TLOU preparada encontrada." :
                "Selecione uma pasta preparada. Não é possível iniciar usando somente o eboot.bin ou PKG.");
    }

    private void launch() {
        File dir = new File(folder());
        if (!new File(dir,"boot-linked.bin").isFile() ||
            !new File(dir,"content.bin").isFile() ||
            !new File(dir,"tlou-view").isDirectory()) {
            refresh();
            alert("TLOU não preparado", "A pasta selecionada precisa conter boot-linked.bin, content.bin e tlou-view. Execute android/tlou_bionic.py sobre seus próprios arquivos do CUSA00552.");
            return;
        }
        if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager() &&
            dir.getAbsolutePath().startsWith("/storage/")) {
            alert("Permissão necessária", "Conceda acesso aos arquivos antes de iniciar o jogo.");
            return;
        }
        getPreferences(0).edit().putString("prepared_path",dir.getAbsolutePath()).apply();
        Intent game = new Intent(this, GameActivity.class);
        game.putExtra("prepared_path",dir.getAbsolutePath());
        startActivity(game);
    }

    @Override protected void onResume() { super.onResume(); refresh(); }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode,resultCode,data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = Objects.requireNonNull(data.getData());
        if (requestCode == PICK_PREPARED) {
            String full = selectedFilesystemPath(uri);
            if (full == null) {
                alert("Pasta selecionada", "Não foi possível converter este URI em caminho de arquivo. Cole o caminho completo da pasta preparada no campo acima.");
                return;
            }
            path.setText(full);
            getPreferences(0).edit().putString("prepared_path",full).apply();
            refresh();
        } else if (requestCode == PICK_DRIVER) {
            try {
                File dir = new File(getFilesDir(), "driver");
                if (!dir.exists() && !dir.mkdirs()) throw new IllegalStateException("Falha ao criar diretório");
                File file = new File(dir,"custom-driver.so");
                try (InputStream in = getContentResolver().openInputStream(uri);
                     FileOutputStream out = new FileOutputStream(file)) {
                    if (in == null) throw new IllegalStateException("Driver ilegível");
                    byte[] bytes = new byte[64*1024];
                    int count;
                    while ((count=in.read(bytes))!=-1) out.write(bytes,0,count);
                }
                if (file.length() < 128) throw new IllegalStateException("Arquivo de driver inválido");
                getPreferences(0).edit().putString("driver_path",file.getAbsolutePath()).apply();
                alert("Driver", "Driver copiado. Compatibilidade Vulkan e dependências serão verificadas na inicialização; o driver de sistema é usado quando nenhum é selecionado.");
            } catch (Exception ex) { alert("Falha ao importar driver",String.valueOf(ex)); }
        }
    }

    private String selectedFilesystemPath(Uri uri) {
        if (!"com.android.externalstorage.documents".equals(uri.getAuthority())) return null;
        try {
            String id = DocumentsContract.getTreeDocumentId(uri);
            if (id.startsWith("primary:")) return "/storage/emulated/0/" + id.substring(8);
            int colon = id.indexOf(':');
            if (colon > 0) return "/storage/" + id.substring(0,colon) + "/" + id.substring(colon+1);
        } catch (Exception ignored) {}
        return null;
    }
}

package ro.cobrabm.fexdroid;

import android.app.Activity;
import android.app.AlertDialog;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import java.util.ArrayList;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
@Config(sdk=28, application=android.app.Application.class)
public class GameSetupDialogTest {
    private void buttons(View view,List<Button> out) {
        if(view instanceof Button)out.add((Button)view);
        if(view instanceof ViewGroup)for(int i=0;i<((ViewGroup)view).getChildCount();i++)
            buttons(((ViewGroup)view).getChildAt(i),out);
    }
    @Test public void allSetupActionsRemainVisibleAndRouteCorrectly() {
        try(var controller=Robolectric.buildActivity(Activity.class).setup()) {
            Activity activity=controller.get();
            for(int expected=0;expected<3;expected++) {
                int[] actual={-1};
                AlertDialog dialog=GameSetupDialog.show(activity,i->actual[0]=i);
                List<Button> actions=new ArrayList<>();
                buttons(dialog.getWindow().getDecorView(),actions);
                actions.removeIf(v->v.getVisibility()!=View.VISIBLE);
                assertEquals(3,actions.size());
                for(Button action:actions)assertTrue(action.isShown());
                actions.get(expected).performClick();
                assertEquals(expected,actual[0]);assertFalse(dialog.isShowing());
            }
        }
    }
    @Test public void setupActionsAreTranslatedInEverySupportedLanguage() {
        try(var controller=Robolectric.buildActivity(Activity.class).setup()) {
            Activity activity=controller.get();
            for(String lang:TlouLanguage.CODES) {
                PortSettings.prefs(activity).edit().putString("ui_language",lang).commit();
                AlertDialog dialog=GameSetupDialog.show(activity,i->{});
                List<Button> actions=new ArrayList<>();buttons(dialog.getWindow().getDecorView(),actions);
                actions.removeIf(v->v.getVisibility()!=View.VISIBLE);
                assertEquals(3,actions.size());
                assertFalse(actions.get(0).getText().toString().isEmpty());
                if(lang.equals("en"))assertEquals("Install PKG",actions.get(0).getText().toString());
                dialog.dismiss();
            }
        }
    }
}

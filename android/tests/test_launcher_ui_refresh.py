"""Regression checks that the real TLOU launcher has the redesigned layout."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
HOME=ROOT/"android-launcher/app/src/main/java/ro/cobrabm/fexdroid/MainActivity.java"
GRADLE=ROOT/"android-launcher/app/build.gradle"
DIALOG=ROOT/"android-launcher/app/src/main/java/ro/cobrabm/fexdroid/GameSetupDialog.java"


class LauncherRedesignTests(unittest.TestCase):
    def test_layout_order_and_new_title(self):
        text=HOME.read_text(encoding="utf-8")
        start=text.index("private void buildHome()")
        stop=text.index("private File baseForUpdate()",start)
        home=text[start:stop]
        self.assertIn('TextView gameTitle=text("THE LAST OF US",31',home)
        self.assertIn('heroTitle.addView(gameTitle)',home)
        self.assertIn('artFrame.addView(heroTitle',home)
        self.assertIn('playButton(root,()->withAccess(()->prepareSelected(true)))',home)
        self.assertLess(home.index("artFrame.addView(heroTitle"),home.index("playButton(root,"))
        self.assertLess(home.index("playButton(root,"),home.index("root.addView(tabs,tabLP)"))
        self.assertNotIn('add(game,"JOGAR"',home)

    def test_build_identity_and_onboarding(self):
        gradle=GRADLE.read_text(encoding="utf-8")
        self.assertIn("versionCode 41",gradle)
        self.assertIn("versionName '0.1.2'",gradle)
        self.assertIn("TLOU UI R41",DIALOG.read_text(encoding="utf-8"))
        self.assertIn("TLOU UI R41",HOME.read_text(encoding="utf-8"))


if __name__=="__main__":
    unittest.main()

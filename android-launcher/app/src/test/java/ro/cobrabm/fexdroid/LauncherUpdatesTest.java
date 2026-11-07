package ro.cobrabm.fexdroid;
import org.junit.Test;
import static org.junit.Assert.*;
public final class LauncherUpdatesTest {
    @Test public void ignoresSameVersion() {
        assertFalse(LauncherUpdates.isNewer("0.1","v0.1-android-bionic"));
    }
    @Test public void newerRevision() {
        assertTrue(LauncherUpdates.isNewer("0.1","v0.1.1-android-bionic"));
        assertTrue(LauncherUpdates.isNewer("0.1.1","v0.2-android-bionic"));
        assertFalse(LauncherUpdates.isNewer("0.2","v0.1.9"));
    }
    @Test public void rejectNonVersionTags() {
        assertFalse(LauncherUpdates.isNewer("0.1","nightly-main"));
        assertFalse(LauncherUpdates.isNewer("0.1","v999999999999999999999999"));
    }
}

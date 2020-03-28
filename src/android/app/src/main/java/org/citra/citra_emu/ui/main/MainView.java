package org.citra.citra_emu.ui.main;

import android.database.Cursor;

/**
 * Abstraction for the screen that shows on application launch.
 * Implementations will differ primarily to target touch-screen
 * or non-touch screen devices.
 */
public interface MainView {
    /**
     * Pass the view the native library's version string. Displaying
     * it is optional.
     *
     * @param version A string pulled from native code.
     */
    void setVersionString(String version);

    /**
     * Tell the view to refresh its contents.
     */
    void refresh();

    void launchSettingsActivity(String menuTag);

    void launchFileListActivity();

    /**
     * To be called when an asynchronous database read completes. Passes the
     * result, in this case a {@link Cursor} to the view.
     *
     * @param games A Cursor containing the games read from the database.
     */
    void showGames(Cursor games);
}

package org.citra.citra_emu.ui.platform;

import android.database.Cursor;

/**
 * Abstraction for a screen representing a single platform's games.
 */
public interface PlatformGamesView {
    /**
     * Tell the view to refresh its contents.
     */
    void refresh();

    /**
     * Tell the view that a certain game's screenshot has been updated,
     * and should be redrawn on-screen.
     *
     * @param position The index of the game that should be redrawn.
     */
    void refreshScreenshotAtPosition(int position);

    /**
     * To be called when an asynchronous database read completes. Passes the
     * result, in this case a {@link Cursor}, to the view.
     *
     * @param games A Cursor containing the games read from the database.
     */
    void showGames(Cursor games);
}

package org.citra.citra_emu.model;

import android.content.ContentValues;
import android.database.Cursor;
import android.os.Environment;

import java.nio.file.Paths;

public final class Game {
    private static final String PATH_SCREENSHOT_FOLDER =
            "file://" + Environment.getExternalStorageDirectory().getPath() + "/citra-emu/ScreenShots/";

    private String mTitle;
    private String mDescription;
    private String mPath;
    private String mGameId;
    private String mScreenshotPath;
    private String mCompany;

    private int mCountry;

    public Game(String title, String description, int country, String path,
                String gameId, String company, String screenshotPath) {
        mTitle = title;
        mDescription = description;
        mCountry = country;
        mPath = path;
        mGameId = gameId;
        mCompany = company;
        mScreenshotPath = screenshotPath;
    }

    public static ContentValues asContentValues(String title, String description,
                                                int country, String path, String gameId, String company) {
        ContentValues values = new ContentValues();

        if (gameId.isEmpty()) {
            // Homebrew, etc. may not have a game ID, use filename as a unique identifier
            gameId = Paths.get(path).getFileName().toString();
        }

        String screenPath = PATH_SCREENSHOT_FOLDER + gameId + "/" + gameId + "-1.png";

        values.put(GameDatabase.KEY_GAME_TITLE, title);
        values.put(GameDatabase.KEY_GAME_DESCRIPTION, description);
        values.put(GameDatabase.KEY_GAME_COUNTRY, country);
        values.put(GameDatabase.KEY_GAME_PATH, path);
        values.put(GameDatabase.KEY_GAME_ID, gameId);
        values.put(GameDatabase.KEY_GAME_COMPANY, company);
        values.put(GameDatabase.KEY_GAME_SCREENSHOT_PATH, screenPath);

        return values;
    }

    public static Game fromCursor(Cursor cursor) {
        return new Game(cursor.getString(GameDatabase.GAME_COLUMN_TITLE),
                cursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION),
                cursor.getInt(GameDatabase.GAME_COLUMN_COUNTRY),
                cursor.getString(GameDatabase.GAME_COLUMN_PATH),
                cursor.getString(GameDatabase.GAME_COLUMN_GAME_ID),
                cursor.getString(GameDatabase.GAME_COLUMN_COMPANY),
                cursor.getString(GameDatabase.GAME_COLUMN_SCREENSHOT_PATH));
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDescription() {
        return mDescription;
    }

    public String getCompany() {
        return mCompany;
    }

    public int getCountry() {
        return mCountry;
    }

    public String getPath() {
        return mPath;
    }

    public String getGameId() {
        return mGameId;
    }

    public String getScreenshotPath() {
        return mScreenshotPath;
    }
}

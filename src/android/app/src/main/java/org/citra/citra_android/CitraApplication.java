// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_android;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import org.citra.citra_android.model.GameDatabase;
import org.citra.citra_android.services.DirectoryInitializationService;
import org.citra.citra_android.utils.PermissionsHandler;

public class CitraApplication extends Application {
    public static GameDatabase databaseHelper;
    private static CitraApplication application;

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = getString(R.string.app_notification_channel_name);
            String description = getString(R.string.app_notification_channel_description);
            int importance = NotificationManager.IMPORTANCE_DEFAULT;
            NotificationChannel channel = new NotificationChannel(getString(R.string.app_notification_channel_id), name, importance);
            channel.setDescription(description);
            // Register the channel with the system; you can't change the importance
            // or other notification behaviors after this
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        application = this;

        if (PermissionsHandler.hasWriteAccess(getApplicationContext())) {
            DirectoryInitializationService.startService(getApplicationContext());
        }

        createNotificationChannel();

        databaseHelper = new GameDatabase(this);
    }

    public static Context getAppContext()
    {
        return application.getApplicationContext();
    }
}

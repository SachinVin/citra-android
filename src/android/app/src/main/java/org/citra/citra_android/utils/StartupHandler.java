package org.citra.citra_android.utils;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.view.View;

import org.citra.citra_android.R;
import org.citra.citra_android.activities.EmulationActivity;

public final class StartupHandler {
    private static void handlePermissionsCheck(FragmentActivity parent) {
        // Ask the user to grant write permission if it's not already granted
        PermissionsHandler.checkWritePermission(parent);

        String start_file = "";
        Bundle extras = parent.getIntent().getExtras();
        if (extras != null) {
            start_file = extras.getString("AutoStartFile");
        }

        if (!TextUtils.isEmpty(start_file)) {
            // Start the emulation activity, send the ISO passed in and finish the main activity
            Intent emulation_intent = new Intent(parent, EmulationActivity.class);
            emulation_intent.putExtra("SelectedGame", start_file);
            parent.startActivity(emulation_intent);
            parent.finish();
        }
    }

    public static void HandleInit(FragmentActivity parent) {
        if (PermissionsHandler.isFirstBoot(parent)) {
            // Prompt user with standard first boot disclaimer
            AlertDialog.Builder builder = new AlertDialog.Builder(parent);
            builder.setTitle(R.string.app_name);
            builder.setIcon(R.drawable.ic_launcher);
            builder.setMessage(parent.getResources().getString(R.string.app_disclaimer));
            builder.setPositiveButton("OK", null);

            builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                public void onDismiss(DialogInterface dialog) {
                    // Ensure user agrees to any necessary app permissions
                    handlePermissionsCheck(parent);

                    // Immediately prompt user to select a game directory on first boot
                    View view = parent.findViewById(R.id.button_add_directory);
                    view.callOnClick();
                }
            });

            builder.show();
        }
    }
}

package org.citra.citra_emu.viewholders;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.citra.citra_emu.R;

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public class GameViewHolder extends RecyclerView.ViewHolder {
    public ImageView imageScreenshot;
    public TextView textGameTitle;
    public TextView textCompany;
    public TextView textFileName;

    public String gameId;

    // TODO Not need any of this stuff. Currently only the properties dialog needs it.
    public String path;
    public String title;
    public String description;
    public int country;
    public String company;
    public String screenshotPath;

    public GameViewHolder(View itemView) {
        super(itemView);

        itemView.setTag(this);

        imageScreenshot = itemView.findViewById(R.id.image_game_screen);
        textGameTitle = itemView.findViewById(R.id.text_game_title);
        textCompany = itemView.findViewById(R.id.text_company);
        textFileName = itemView.findViewById(R.id.text_filename);
    }
}

package org.citra.citra_emu.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.citra.citra_emu.R;

import java.io.IOException;

import androidx.annotation.Nullable;

public class PicassoUtils {
    public static void loadGameIcon(ImageView imageView, String gamePath) {
        Picasso picassoInstance = new Picasso.Builder(imageView.getContext())
                .addRequestHandler(new GameIconRequestHandler())
                .build();

        picassoInstance
                .load(Uri.parse("iso:/" + gamePath))
                .noFade()
                .noPlaceholder()
                .fit()
                .centerInside()
                .config(Bitmap.Config.RGB_565)
                .error(R.drawable.no_icon)
                .transform(new PicassoRoundedCornersTransformation())
                .into(imageView);
    }

    // Blocking call. Load image from file and crop/resize it to fit in width x height.
    @Nullable
    public static Bitmap LoadBitmapFromFile(String uri, int width, int height) {
        try {
            return Picasso.get()
                    .load(Uri.parse(uri))
                    .config(Bitmap.Config.ARGB_8888)
                    .centerCrop()
                    .resize(width, height)
                    .get();
        } catch (IOException e) {
            return null;
        }
    }
}

package org.citra.citra_emu.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.citra.citra_emu.R;

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
}

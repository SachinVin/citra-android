package org.citra.citra_android.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.citra.citra_android.R;

public class PicassoUtils {
    public static void loadGameBanner(ImageView imageView, String gamePath) {
        Picasso picassoInstance = new Picasso.Builder(imageView.getContext())
                .addRequestHandler(new GameBannerRequestHandler())
                .build();

        picassoInstance
                .load(Uri.parse("iso:/" + gamePath))
                .noFade()
                .noPlaceholder()
                .fit()
                .centerInside()
                .config(Bitmap.Config.RGB_565)
                .error(R.drawable.no_banner)
                .into(imageView);
    }
}

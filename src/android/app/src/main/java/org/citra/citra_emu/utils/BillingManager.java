package org.citra.citra_emu.utils;

import android.app.Activity;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.Purchase.PurchasesResult;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.SkuDetails;
import com.android.billingclient.api.SkuDetailsParams;

import java.util.ArrayList;
import java.util.List;

public class BillingManager implements PurchasesUpdatedListener {
    private final String BILLING_SKU_PREMIUM = "android.test.purchased";

    private final Activity mActivity;
    private BillingClient mBillingClient;
    private SkuDetails mSkuPremium;
    private boolean mIsPremiumActive = false;
    private boolean mIsServiceConnected = false;
    private Runnable mUpdateBillingCallback;

    public BillingManager(Activity activity) {
        mActivity = activity;
        mBillingClient = BillingClient.newBuilder(mActivity).enablePendingPurchases().setListener(this).build();
        querySkuDetails();
    }

    /**
     * @return true if Premium subscription is currently active
     */
    public boolean isPremiumActive() {
        return mIsPremiumActive;
    }

    /**
     * Invokes the billing flow for Premium
     *
     * @param callback Optional callback, called once, on completion of billing
     */
    public void invokePremiumBilling(Runnable callback) {
        if (mSkuPremium == null) {
            return;
        }

        // Optional callback to refresh the UI for the caller when billing completes
        mUpdateBillingCallback = callback;

        // Invoke the billing flow
        BillingFlowParams flowParams = BillingFlowParams.newBuilder()
                .setSkuDetails(mSkuPremium)
                .build();
        mBillingClient.launchBillingFlow(mActivity, flowParams);
    }

    @Override
    public void onPurchasesUpdated(BillingResult billingResult, List<Purchase> purchaseList) {
        if (purchaseList == null || purchaseList.isEmpty()) {
            // Premium is not active, or billing is unavailable
            return;
        }

        Purchase premiumPurchase = null;
        for (Purchase purchase : purchaseList) {
            if (purchase.getSku().equals(BILLING_SKU_PREMIUM)) {
                premiumPurchase = purchase;
            }
        }

        if (premiumPurchase != null) {
            // Premium has been purchased
            mIsPremiumActive = true;

            if (mUpdateBillingCallback != null) {
                try {
                    mUpdateBillingCallback.run();
                } catch (Exception e) {
                    e.printStackTrace();
                }
                mUpdateBillingCallback = null;
            }
        }
    }

    private void onQuerySkuDetailsFinished(List<SkuDetails> skuDetailsList) {
        if (skuDetailsList == null) {
            // This can happen when no user is signed in
            return;
        }

        if (skuDetailsList.isEmpty()) {
            return;
        }

        mSkuPremium = skuDetailsList.get(0);

        queryPurchases();
    }

    private void querySkuDetails() {
        Runnable queryToExecute = new Runnable() {
            @Override
            public void run() {
                SkuDetailsParams.Builder params = SkuDetailsParams.newBuilder();
                List<String> skuList = new ArrayList<>();

                skuList.add(BILLING_SKU_PREMIUM);
                params.setSkusList(skuList).setType(BillingClient.SkuType.INAPP);

                mBillingClient.querySkuDetailsAsync(params.build(),
                        (billingResult, skuDetailsList) -> onQuerySkuDetailsFinished(skuDetailsList));
            }
        };

        executeServiceRequest(queryToExecute);
    }

    private void onQueryPurchasesFinished(PurchasesResult result) {
        // Have we been disposed of in the meantime? If so, or bad result code, then quit
        if (mBillingClient == null || result.getResponseCode() != BillingClient.BillingResponseCode.OK) {
            return;
        }
        // Update the UI and purchases inventory with new list of purchases
        onPurchasesUpdated(result.getBillingResult(), result.getPurchasesList());
    }

    private void queryPurchases() {
        Runnable queryToExecute = new Runnable() {
            @Override
            public void run() {
                final PurchasesResult purchasesResult = mBillingClient.queryPurchases(BillingClient.SkuType.INAPP);
                onQueryPurchasesFinished(purchasesResult);
            }
        };

        executeServiceRequest(queryToExecute);
    }

    private void startServiceConnection(final Runnable executeOnFinish) {
        mBillingClient.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(BillingResult billingResult) {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    mIsServiceConnected = true;
                }

                if (executeOnFinish != null) {
                    executeOnFinish.run();
                }
            }

            @Override
            public void onBillingServiceDisconnected() {
                mIsServiceConnected = false;
            }
        });
    }

    private void executeServiceRequest(Runnable runnable) {
        if (mIsServiceConnected) {
            runnable.run();
        } else {
            // If billing service was disconnected, we try to reconnect 1 time.
            startServiceConnection(runnable);
        }
    }
}

package org.citra.citra_android.ui.platform;

import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.citra.citra_android.R;
import org.citra.citra_android.adapters.GameAdapter;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView {
    private static final String ARG_PLATFORM = "platform";

    private PlatformGamesPresenter mPresenter = new PlatformGamesPresenter(this);

    private GameAdapter mAdapter;
    private RecyclerView mRecyclerView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_grid, container, false);

        findViews(rootView);

        mPresenter.onCreateView();

        return rootView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        int columns = getResources().getInteger(R.integer.game_grid_columns);
        RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(), columns);
        mAdapter = new GameAdapter();

        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.setAdapter(mAdapter);

        mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(8));
    }

    @Override
    public void refreshScreenshotAtPosition(int position) {
        mAdapter.notifyItemChanged(position);
    }

    @Override
    public void refresh() {
        mPresenter.refresh();
    }

    @Override
    public void onItemClick(String gameId) {
        // No-op for now
    }

    @Override
    public void showGames(Cursor games) {
        if (mAdapter != null) {
            mAdapter.swapCursor(games);
        }
    }

    private void findViews(View root) {
        mRecyclerView = root.findViewById(R.id.grid_games);
    }
}

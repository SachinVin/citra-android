package org.citra.citra_emu.ui.platform;

import android.database.Cursor;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.adapters.GameAdapter;
import org.citra.citra_emu.model.GameDatabase;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView {
    private PlatformGamesPresenter mPresenter = new PlatformGamesPresenter(this);

    private GameAdapter mAdapter;
    private RecyclerView mRecyclerView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

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
        mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(1));

        // Add swipe down to refresh gesture
        final SwipeRefreshLayout pullToRefresh = view.findViewById(R.id.refresh_grid_games);
        pullToRefresh.setOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                GameDatabase databaseHelper = CitraApplication.databaseHelper;
                databaseHelper.scanLibrary(databaseHelper.getWritableDatabase());
                refresh();
                pullToRefresh.setRefreshing(false);
            }
        });
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

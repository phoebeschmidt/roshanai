package com.medeagames.myapplication;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.SoundEffectConstants;
import android.view.View;

/**
 * Created by carolyn on 5/9/16.
 */
public class SingleColorView extends View {
    static final int mOffColor    = Color.WHITE; //0xFFFFFFFF; //R.color.offColor;
    static final int mOnColor     = Color.RED; //0xFFFF0000; //R.color.onColor;
    static final int mBorderColor = 0xFF000000; //R.color.cellBorderColor;  // XXX fix this - get from resources

    int mColor = mOffColor;
    Paint mCellPaint;
    Paint mBorderPaint;
    GestureDetector mDetector;

    public SingleColorView(MainActivity mainActivity) {
        super(mainActivity);
        init(mainActivity.getBaseContext());
    }

    class mListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }
    }

    public SingleColorView(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Initial color is as defined in the styled attributes
        TypedArray a = context.getTheme().obtainStyledAttributes(
                attrs,
                R.styleable.SingleColorView,
                0, 0);

        try {
            mColor = mOffColor; //a.getColor(R.styleable.SingleColorView_default_cell_color, getResources().getColor(mOffColor));
        } finally {
            a.recycle();
        }

        init(context);
    }

    private void init(Context context) {
        // create paints!
        mCellPaint = new Paint(0);
        mCellPaint.setStyle(Paint.Style.FILL);
        mCellPaint.setColor(mColor);

        mBorderPaint = new Paint(0);
        mBorderPaint.setStyle(Paint.Style.STROKE);
        mBorderPaint.setStrokeWidth(5);
        mBorderPaint.setColor(mBorderColor);//getResources().getColor(mBorderColor));

        // create gesture detector
        mDetector = new GestureDetector(context, new mListener());
    }

    public boolean isOn() {
        return mColor == mOnColor;
    }


    public void turnOn(boolean onOff) {
        if (onOff && mColor != mOnColor) {
            mColor = mOnColor;
            invalidate();
        } else if (!onOff && mColor != mOffColor) {
            mColor = mOffColor;
            invalidate();
        }
        mCellPaint.setColor(mColor);
    }

    protected void toggle() {
        if (mColor == mOnColor) {
            mColor = mOffColor;
        } else {
            mColor = mOnColor;
        }
        mCellPaint.setColor(mColor);
        invalidate();
        playSoundEffect(SoundEffectConstants.CLICK);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        int height = this.getHeight();
        int width = this.getWidth();
        //int height = R.dimen.onOff_cell_height;
        //int width  = R.dimen.onOff_cell_width;
        canvas.drawRect(0,0, width, height, mCellPaint);
        canvas.drawRect(0,0, width, height, mBorderPaint);
        //canvas.drawRect(0,0, 150, 50, mBorderPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean result = mDetector.onTouchEvent(event);
        if (!result) {
            if (event.getAction() == MotionEvent.ACTION_UP) {
                toggle();
                result = true;
            }
        }
        return result;
    }
}

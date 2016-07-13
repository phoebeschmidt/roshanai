package com.medeagames.testProportional;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Collections;
import java.util.Comparator;
import java.util.Stack;

/**
 * Created by carolyn on 6/27/16.
 */
public class ProportionalView extends View {

    private static final int UNDO_SIZE = 10;
    protected List<PointF> mPointsArray = new ArrayList<>();
    protected Paint mLinePaint;
    protected Paint mPointPaint;
    protected LinkedList<List<PointF>> mPointsStack = new LinkedList<>();
    private PointF mSelectedPoint;
    private PointF mLastPoint;
    private PointF mFirstPoint;
    protected GestureDetector mDetector;
    private boolean mMovingPoint = false;

    private static Comparator<PointF> mPointFComparator = new Comparator<PointF>() {
        @Override
        public int compare(PointF lhs, PointF rhs) {
            if (lhs.x > rhs.x) {
                return 1;
            } else if (lhs.x < rhs.x) {
                return -1;
            }
            return 0;
        }
    };

    static final int lineColor = Color.BLACK;
    static final int pointColor = Color.RED;
    static final double DELETE_RADIUS = 60;
    static final double MODIFY_RADIUS = 60;
    static final double ADD_RADIUS    = 20;
    static final double MIN_FLING     = 100;

    public ProportionalView(MainActivity mainActivity)
    {
        super(mainActivity);
        init(mainActivity.getBaseContext());

    }

    public ProportionalView(Context context, AttributeSet attrs)
    {
        super(context, attrs);
        init(context);
    }

    private void init(Context context)
    {
        // set up paints and gesture detector
        mLinePaint = new Paint(0);
        mLinePaint.setStyle(Paint.Style.STROKE);
        mLinePaint.setStrokeWidth(5);
        mLinePaint.setColor(lineColor);

        mPointPaint = new Paint(0);
        mPointPaint.setStyle(Paint.Style.FILL);
        mPointPaint.setColor(pointColor);

        // create gesture detector
        mDetector = new GestureDetector(context, new mListener(this));

        // and the two default points
        setDefaultPoints();

    }

    class mListener extends GestureDetector.SimpleOnGestureListener {
        View parent;

        public mListener(View parent) {
            super();
            this.parent = parent;
        }

        @Override
        public void onLongPress(MotionEvent e) {
            Log.i("FLG", "Long press event");
            PointF target = getClosePoint(e.getX(), e.getY(), MODIFY_RADIUS);
            if (target != null) {
                mSelectedPoint = target;
                Log.i("FLG", "Selected point");
            }
        }

        @Override
        public boolean onFling(MotionEvent downEvent, MotionEvent upEvent, float vx, float vy) {
            // If we're currently editing a point, don't delete
            if (mSelectedPoint == null) {
                // If we're close to a point, delete it
                Log.i("FLG", "Fling event");
                if (getSquaredDistance(downEvent.getX(), downEvent.getY(), upEvent.getX(), upEvent.getY()) > MIN_FLING) {
                    Log.i("FLG", "Big enough for a Fling!");
                    PointF target = getClosePoint(downEvent.getX(), downEvent.getY(), DELETE_RADIUS);
                    if (target != null && target != mLastPoint && target != mFirstPoint) {
                        Log.i("FLG", "Deleting point");
                        savePointsList();
                        mPointsArray.remove(target);
                        invalidate();
                    } else {
                        Log.i("FLG", "No point to fling");
                    }
                }
            }
            return true;
        }

        @Override
        public boolean onDoubleTap(MotionEvent e) {
            // If we're close to a line (but not too close to an existing point, add a point)
            PointF target = getClosePoint(e.getX(), e.getY(), ADD_RADIUS);
            Log.i("FLG", "doubletap");
            if (target != null) {
                Log.e("FLG", "Too close to other point to add new one");
            } else {
                // place point in grid
                savePointsList();
                PointF newPoint = new PointF(e.getX() / parent.getWidth(), e.getY() / parent.getHeight());
                mPointsArray.add(newPoint);
                Collections.sort(mPointsArray, mPointFComparator); // XXX don't allow points outside the view, and make sure they're all within the start/end range
                Log.i("FLG", "adding point");
                invalidate();
            }
            return  true;
        }
    }

    private double getSquaredDistance(double x1, double y1, double x2, double y2) {
        return Math.pow(x1-x2, 2.0) + Math.pow(y1-y2, 2.0);
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        int canvasWidth  = canvas.getWidth();
        int canvasHeight = canvas.getHeight();
        int pointWidth   = 30;
        int pointHeight  = 30; // XXX should be in resources

        // fill background with blue-green, for the moment...
        canvas.drawColor(Color.rgb(128,200,234));

        // draw from point to point, in order
        int nPoints = mPointsArray.size();
        if (nPoints == 0) {
            // no points. just do 50% from start to finish
            float[] pts = {0, canvasHeight/2, canvasWidth, canvasHeight/2};
            canvas.drawLines(pts, mLinePaint);
        } else {
            float[] pts = {0, mPointsArray.get(0).y * canvasHeight, 0, 0};
            for (PointF point : mPointsArray) {
                float pointX = point.x * canvasWidth;
                float pointY = point.y * canvasHeight;
                pts[2] = pointX;
                pts[3] = pointY;
                canvas.drawLines(pts, mLinePaint);
                pts[0] = pts[2];
                pts[1] = pts[3];
            }
            // draw control points on top
            for (PointF point : mPointsArray) {
                float pointX = point.x * canvasWidth;
                float pointY = point.y * canvasHeight;
                canvas.drawRect(pointX-pointWidth/2, pointY-pointHeight/2, pointX+pointWidth/2, pointY+pointHeight/2, mPointPaint);
            }
        }
    }

    // gets the nearest point within the radius suggested
    protected PointF getClosePoint(float X, float Y, double radius)
    {
        double closestDistance = -1;
        PointF nearestPoint = null;
        int viewWidth  = this.getWidth();
        int viewHeight = this.getHeight();
        double radiusSquared = radius*radius;
        for (PointF point : mPointsArray) {
            double distance = Math.pow(X - point.x * viewWidth, 2) + Math.pow(Y - point.y * viewHeight, 2);
            if (distance <= radiusSquared && distance > closestDistance) {
                closestDistance = distance;
                nearestPoint = point;
            }
        }
        return nearestPoint;
    }


    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        boolean result = mDetector.onTouchEvent(event);
        if (!result) {
            int eventType = event.getAction();
            //Log.i("FLG", "Event of type " + event.toString());
            if (eventType == MotionEvent.ACTION_MOVE) {
                if (mSelectedPoint != null) {
                    if (!mMovingPoint) {
                        mMovingPoint = true;
                        int selectedIdx = mPointsArray.indexOf(mSelectedPoint);
                        savePointsList();
                        mSelectedPoint = mPointsArray.get(selectedIdx);
                    }
                    float xPos = event.getX()/this.getWidth();
                    float yPos = event.getY()/this.getHeight();
                    if (xPos <= 0.0f) {
                        xPos = 0.01f;
                    } else if (xPos >= 1.0f) {
                        xPos = 0.99f;
                    }
                    if (yPos <= 0.0f) {
                        yPos = 0.0f;
                    } else if (yPos >= 1.0f) {
                        yPos = 1.0f;
                    }
                    if (mSelectedPoint != mFirstPoint && mSelectedPoint != mLastPoint) {
                        mSelectedPoint.set(xPos, yPos);
                    } else {
                        mSelectedPoint.y = yPos;  // X coord of the first and last points are fixed
                    }
                    Collections.sort(mPointsArray, mPointFComparator);
                    invalidate();
                }
            } if (eventType == MotionEvent.ACTION_UP) {
                mSelectedPoint = null;
                mMovingPoint = false;
            }
        }
        return true; // just eating the event for now

    }

    public List<PointF> getPoints()
    {
        return mPointsArray;
    }

    public void setPoints(List<PointF> points)
    {
        this.mPointsArray = points;
        if (points.size() > 1) {
            mFirstPoint = points.get(0);
            mLastPoint = points.get(points.size()-1);
        }
    }

    public void clear()
    {
        setDefaultPoints();
    }

    // Two anchor points on the ends...
    private void setDefaultPoints()
    {
        mPointsArray = new ArrayList<PointF>();
        mFirstPoint = new PointF(0f, 0.5f);
        mLastPoint  = new PointF(1.0f, 0.5f);
        mPointsArray.add(mFirstPoint);
        mPointsArray.add(mLastPoint);
        invalidate();

    }

    public boolean canUndo() {
        return !mPointsStack.isEmpty();
    }

    public void undo() {
        if (mPointsStack.isEmpty()) {
            return;
        }
        List<PointF> oldPoints = mPointsStack.pop();
        setPoints(oldPoints);
        invalidate();
    }

    private List<PointF> pointsListCopy() {
        List<PointF> listCopy = new ArrayList<>();
        for (PointF point : mPointsArray) {
            PointF newPoint = new PointF(point.x, point.y);
            listCopy.add(newPoint);
        }
        return listCopy;
    }

    private void savePointsList() {
        mPointsStack.push(mPointsArray);
        mPointsArray = pointsListCopy();
        if (mPointsStack.size() > UNDO_SIZE) {
            mPointsStack.removeLast();
        }
        mFirstPoint = mPointsArray.get(0);
        mLastPoint  = mPointsArray.get(mPointsArray.size()-1);
    }
}

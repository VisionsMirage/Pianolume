#pragma once

#include <juce_events/juce_events.h>
#include "ProjectionCalibration.h"

namespace pianolume
{

/** 合わせ込みの状態。調整画面と投影画面が共有し、変更をメッセージスレッドへ通知する。

    getCalibration() は描画スレッドからも呼べる。それ以外はメッセージスレッドから呼ぶ。
*/
class CalibrationModel : public juce::ChangeBroadcaster
{
public:
    enum class Handle
    {
        fallTopLeft, fallTopRight, fallBottomRight, fallBottomLeft,
        keyTopLeft, keyTopRight, keyBottomRight, keyBottomLeft,
        none
    };

    ProjectionCalibration getCalibration() const;
    void setCalibration (const ProjectionCalibration& newCalibration);

    /** 操作できるハンドル。面を連動させているときは、境目の 2 点は落下エリア側だけになる。 */
    std::vector<Handle> getEditableHandles() const;
    juce::Point<float> getHandlePosition (Handle handle) const;
    void moveHandle (Handle handle, juce::Point<float> position);

    void setSelectedHandle (Handle handle);
    Handle getSelectedHandle() const noexcept           { return selectedHandle.load(); }
    void selectNextHandle();
    void nudgeSelectedHandle (juce::Point<float> delta);

    void setLinkPlanes (bool shouldLink);
    void flipHorizontally();
    void flipVertically();
    void rotate180();
    void resetCorners();

    /** 面を、左上・右上・左下の角を基準に、水平・垂直の辺からなる長方形にそろえる。
        落下エリアは 1・2・4、鍵盤は 4・3・6（連動していないときは 4'・3'・6）が基準になる。 */
    void squarePlane (bool fallPlane);

    void setBlackKeys (float offsetXLeft, float offsetXRight, float offsetY, float length);
    void setOutput (float brightness, float gamma);

    /** 合わせ込み中は投影画面に枠・番号・鍵の輪郭を出す。 */
    void setCalibrating (bool shouldCalibrate);
    bool isCalibrating() const noexcept                 { return calibrating.load(); }

    static juce::String getHandleLabel (Handle handle);
    static bool isFallHandle (Handle handle) noexcept   { return handle <= Handle::fallBottomLeft; }

private:
    template <typename Function>
    void modify (Function&& function);

    void transformAllCorners (const std::function<juce::Point<float> (juce::Point<float>)>& transform);

    mutable juce::SpinLock lock;
    ProjectionCalibration calibration = ProjectionCalibration::createDefault();
    std::atomic<Handle> selectedHandle { Handle::fallTopLeft };
    std::atomic<bool> calibrating { false };
};

} // namespace pianolume

#pragma once

namespace pianolume
{

/** Windows のタイトルバーをダーク表示にし、背景色を画面に合わせる。他の OS では何もしない。 */
void applyDarkTitleBar (void* nativeWindowHandle);

} // namespace pianolume

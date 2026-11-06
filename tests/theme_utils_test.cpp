// Checks the parsers for four-value theme lists: whitespace tolerance, negative clamping and
// the fallback returned for malformed input.

#include "QcwThemeUtils.h"
#include <QCoreApplication>
#include <QMargins>
#include <cstdio>

namespace
{

bool checkMargins(const QString& text, const QMargins& expected, const QMargins& fallback = {})
{
    const QMargins actual = QcwThemeUtils::parseMargins(text, fallback);
    if (actual == expected)
    {
        return true;
    }

    std::fprintf(stderr, "parseMargins(\"%s\") = (%d, %d, %d, %d), expected (%d, %d, %d, %d)\n",
                 text.toLocal8Bit().constData(), actual.left(), actual.top(), actual.right(), actual.bottom(),
                 expected.left(), expected.top(), expected.right(), expected.bottom());
    return false;
}

bool sameRadii(const QcwBorderRadii& left, const QcwBorderRadii& right)
{
    return left.topLeft == right.topLeft && left.topRight == right.topRight && left.bottomRight == right.bottomRight &&
           left.bottomLeft == right.bottomLeft;
}

bool checkRadii(const QString& text, const QcwBorderRadii& expected, const QcwBorderRadii& fallback = {})
{
    const QcwBorderRadii actual = QcwThemeUtils::parseRadii(text, fallback);
    if (sameRadii(actual, expected))
    {
        return true;
    }

    std::fprintf(stderr, "parseRadii(\"%s\") = (%d, %d, %d, %d), expected (%d, %d, %d, %d)\n",
                 text.toLocal8Bit().constData(), actual.topLeft, actual.topRight, actual.bottomRight, actual.bottomLeft,
                 expected.topLeft, expected.topRight, expected.bottomRight, expected.bottomLeft);
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QMargins defaultMargins { 1, 2, 3, 4 };
    const QcwBorderRadii defaultRadii { 5, 6, 7, 8 };

    bool ok = true;

    ok &= checkMargins("6 6 6 6", QMargins { 6, 6, 6, 6 });
    ok &= checkMargins(" 1\t2   3 4 ", QMargins { 4, 1, 2, 3 });
    ok &= checkMargins("-1 2 3 4", QMargins { 4, 0, 2, 3 });
    ok &= checkMargins("6 6", defaultMargins, defaultMargins);
    ok &= checkMargins("6 x 6 6", defaultMargins, defaultMargins);
    ok &= checkMargins(QString(), defaultMargins, defaultMargins);

    ok &= checkRadii("4 4 4 4", QcwBorderRadii { 4, 4, 4, 4 });
    ok &= checkRadii(" 1\t2   3 4 ", QcwBorderRadii { 1, 2, 3, 4 });
    ok &= checkRadii("-1 2 3 4", QcwBorderRadii { 0, 2, 3, 4 });
    ok &= checkRadii("4 4", defaultRadii, defaultRadii);
    ok &= checkRadii("4 x 4 4", defaultRadii, defaultRadii);
    ok &= checkRadii(QString(), defaultRadii, defaultRadii);

    std::fprintf(stderr, "theme_utils=%d\n", ok);
    return ok ? 0 : 1;
}

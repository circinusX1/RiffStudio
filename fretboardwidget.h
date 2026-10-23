#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>

class FretboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit FretboardWidget(QWidget *parent = nullptr);

    void setChord(const QString &currentChord, const QString &nextChord = "");
    void setViewMode(bool isTab);       // true = Tabs, false = Fretboard
    void setFingeringMode(bool isPower);// true = Power, false = Standard

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_currentChord = "G";
    QString m_nextChord = "D";
    bool m_viewTabs = false;
    bool m_powerMode = false;

    struct Shape {
        QVector<QString> stdShape;
        QVector<QString> powerShape;
    };
    QMap<QString, Shape> m_shapes;

    void initShapes();
    QVector<QString> getActiveShape(const QString &chord);
    void drawFretboard(QPainter &painter, const QRect &bounds, const QString &chordName, bool isMini);
};

#ifndef CARDPANEL_H
#define CARDPANEL_H

#include <QWidget>
#include <QPixmap>
#include "card.h"
#include "player.h"

/**
 * @brief 扑克牌控件 —— 单张牌的 GUI 展示
 *
 * 功能：
 * - 显示牌的正面/背面图片（可切换）
 * - 选中状态高亮（金色边框 + 半透明覆盖 + 上移）
 * - 响应鼠标左键点击（发射 clicked() 信号）
 * - 静态方法从资源加载卡牌图片（含程序化 fallback 绘制）
 *
 * 使用场景：
 * - 用户手牌区：正面朝上，可点击选择
 * - 机器人手牌区：背面朝上，仅显示张数
 * - 出牌区：正面朝上，带缩放动画
 * - 底牌区：正面朝上
 */
class CardPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CardPanel(QWidget *parent = nullptr);

    // ============ 图片管理 ============

    /// 设置牌的正面图片（内部自动缩放到控件尺寸）
    void setFrontPixmap(const QPixmap &front);

    /// 设置牌的背面图片
    void setBackPixmap(const QPixmap &back);

    QPixmap getFrontPixmap() const;

    void setThemeFace(const QPixmap &face);
    void setThemeBack(const QPixmap &back);

    // ============ 状态控制 ============

    /// 切换正反面
    void setFrontSide(bool flag);
    bool isFrontSide() const;

    /// 切换选中状态（上移 + 金色高亮边框）
    void setSelected(bool flag);
    bool isSelected() const;

    // ============ 数据绑定 ============

    /// 绑定牌的数值（花色+点数）
    void setCard(const Card &card);
    Card getCard() const;

    /// 设置牌的拥有者
    void setOwner(Player *player);
    Player *getOwner() const;

    // ============ 静态图片加载 ============

    /// 从文件系统加载卡牌正面图片（含程序化 fallback）
    static QPixmap loadCardPixmap(const Card &card);

    /// 加载牌背面图片（含程序化 fallback）
    static QPixmap loadCardBack();

signals:
    /// 左键点击信号
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPixmap m_front;       ///< 正面图片
    QPixmap m_back;        ///< 背面图片
    bool m_isfront;        ///< 是否正面朝上
    bool m_isselected;     ///< 是否被选中
    Card m_card;           ///< 牌的数值
    Player *m_owner;       ///< 牌的拥有者
    QPixmap m_themeFace;
    QPixmap m_themeBack;
};

#endif // CARDPANEL_H

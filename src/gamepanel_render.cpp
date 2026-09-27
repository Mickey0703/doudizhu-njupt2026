#include "gamepanel.h"

void GamePanel::updateAllPlayerCards()
{
    updateUserCards();
    updateRobotCards(m_gameControl->getLeftRobot());
    updateRobotCards(m_gameControl->getRightRobot());
}
void GamePanel::updateUserCards()
{
    const QPixmap theme = !m_online->isOnline() && m_themeManager->isLoaded()
        ? m_themeManager->cardBack() : QPixmap();
    m_tableView->setCardBack(CardPanel::loadCardBack(), theme);
    m_tableView->setHand(m_gameControl->getUserPlayer()->getCards());
}
void GamePanel::updateRobotCards(Player *robot)
{
    const bool left = robot == m_gameControl->getLeftRobot();
    const int count = m_online->isOnline() ? remoteHandCountFor(robot) : robot->getCards().cardCount();
    m_tableView->setOpponentCount(left ? CardTableView::Side::Left : CardTableView::Side::Right, count);
}
void GamePanel::updatePlayArea(Player *player, const Cards &cards)
{
    m_tableView->showPlayed(cards, player && player != m_gameControl->getUserPlayer());
}
void GamePanel::updateLordCards()
{
    m_tableView->showBottom(m_actions->state().revealedBottomCards);
}
void GamePanel::updateLordIcons()
{
    for (const auto &pair : {qMakePair(static_cast<Player *>(m_gameControl->getUserPlayer()),m_userLordIcon),
         qMakePair(static_cast<Player *>(m_gameControl->getLeftRobot()),m_leftLordIcon),
         qMakePair(static_cast<Player *>(m_gameControl->getRightRobot()),m_rightLordIcon)}) {
        pair.second->setVisible(pair.first->getRole() == Player::Lord);
        pair.second->setText(QStringLiteral("👑地主"));
    }
}
void GamePanel::clearPlayArea() { m_tableView->clearPlayed(); }
void GamePanel::clearLordCards() { m_tableView->clearBottom(); }
void GamePanel::arrangeUserCards() { m_tableView->relayout(); }
void GamePanel::arrangeRobotCards(Player *) { m_tableView->relayout(); }
void GamePanel::arrangePlayArea() { m_tableView->relayout(); }
void GamePanel::arrangeLordArea() { m_tableView->relayout(); }

void GamePanel::raiseGameButtons()
{
    // 将所有交互按钮提升到卡片层的最上层，防止被新创建的卡牌遮挡
    if (m_btnScoreboard) m_btnScoreboard->raise();
    m_btnVsAI->raise();       m_btnCreateRoom->raise();    m_btnJoinRoom->raise();
    m_btnStart->raise();      m_btnRestart->raise();       m_btnBackMenu->raise();
    m_btnPlay->raise();       m_btnPass->raise();          m_btnHint->raise();
    m_btnCall1->raise();      m_btnCall2->raise();         m_btnCall3->raise();
    m_btnNoCall->raise();
    if (m_roomIdLabel) m_roomIdLabel->raise();
    if (m_playerListLabel) m_playerListLabel->raise();
    if (m_btnChaosMode) m_btnChaosMode->raise();
    if (m_btnAvatar) m_btnAvatar->raise();
    if (m_themeCombo) m_themeCombo->raise();
    if (m_btnConfirmTheme) m_btnConfirmTheme->raise();
    // 确保文字标签不被卡牌遮挡
    if (m_statusLabel) m_statusLabel->raise();
    if (m_timerLabel) m_timerLabel->raise();
    // 三家信息面板（头像/昵称/分数/地主图标）提到卡牌层之上，避免牌堆顶边压住标签
    // 被裁成细线（此前这几个标签从未 raise，卡牌层盖在其上导致右上角"细红/金线"瑕疵）。
    for (QLabel *lbl : {m_leftAvatar, m_leftNameLabel, m_leftLordIcon,
                        m_rightAvatar, m_rightNameLabel, m_rightLordIcon,
                        m_userAvatar, m_userNameLabel, m_userLordIcon})
        if (lbl) lbl->raise();
    // 横幅最后 raise，保持在所有控件之下（仅视觉展示）
    if (m_titleLogo) m_titleLogo->raise();
    if (m_chaosBanner) m_chaosBanner->raise();

}

void GamePanel::setCallButtonsVisible(bool v)
{
    m_btnCall1->setVisible(v);
    m_btnCall2->setVisible(v);
    m_btnCall3->setVisible(v);
    m_btnNoCall->setVisible(v);
    if (v) raiseGameButtons();
}

void GamePanel::setPlayButtonsVisible(bool v)
{
    m_btnPlay->setVisible(v);
    m_btnPass->setVisible(v);
    m_btnHint->setVisible(v);
    if (v) raiseGameButtons();
}

void GamePanel::setMenuButtonsVisible(bool v)
{
    m_btnVsAI->setVisible(v);
    m_btnCreateRoom->setVisible(v);
    m_btnJoinRoom->setVisible(v);
}

#include "MasterLayout.hpp"
#include "../Compositor.hpp"

SMasterNodeData* CHyprMasterLayout::getNodeFromWindow(CWindow* pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow == pWindow)
            return &nd;
    }

    return nullptr;
}

int CHyprMasterLayout::getNodesOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

int CHyprMasterLayout::getMastersOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

SMasterWorkspaceData* CHyprMasterLayout::getMasterWorkspaceData(const int& ws) {
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_lMasterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    const auto orientation      = &g_pConfigManager->getConfigValuePtr("master:orientation")->strValue;
    if (*orientation == "top") {
        PWORKSPACEDATA->orientation = ORIENTATION_TOP;
    } else if (*orientation == "right") {
        PWORKSPACEDATA->orientation = ORIENTATION_RIGHT;
    } else if (*orientation == "bottom") {
        PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;
    } else {
        PWORKSPACEDATA->orientation = ORIENTATION_LEFT;
    }
    return PWORKSPACEDATA;
}

std::string CHyprMasterLayout::getLayoutName() {
    return "Master";
}

SMasterNodeData* CHyprMasterLayout::getMasterNodeOnWorkspace(const int& ws) {
    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CHyprMasterLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    static auto* const PNEWTOP = &g_pConfigManager->getConfigValuePtr("master:new_on_top")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    const auto         PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();

    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow     = pWindow;

    static auto* const PNEWISMASTER = &g_pConfigManager->getConfigValuePtr("master:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    float              lastSplitPercent   = 0.5f;

    if (*PNEWISMASTER || WINDOWSONWORKSPACE == 1) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster      = false;
                lastSplitPercent = nd.percMaster;
                break;
            }
        }

        PNODE->isMaster   = true;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PMONITOR->vecSize.x * lastSplitPercent || MAXSIZE.y < PMONITOR->vecSize.y) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster = false;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow);
            MAXSIZE.x < PMONITOR->vecSize.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        g_pCompositor->setWindowFullscreen(PFULLWINDOW, false, FULLSCREEN_FULL);
    }

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::onWindowRemovedTiling(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    pWindow->m_sSpecialRenderData.rounding = true;
    pWindow->m_sSpecialRenderData.border   = true;
    pWindow->m_sSpecialRenderData.decorate = true;

    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    const auto MASTERSLEFT = getMastersOnWorkspace(PNODE->workspaceID);

    if (PNODE->isMaster && MASTERSLEFT < 2) {
        // find new one
        for (auto& nd : m_lMasterNodesData) {
            if (!nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster = true;
                break;
            }
        }
    }

    const auto WORKSPACEID = PNODE->workspaceID;

    m_lMasterNodesData.remove(*PNODE);

    if (getMastersOnWorkspace(WORKSPACEID) == getNodesOnWorkspace(WORKSPACEID) && MASTERSLEFT > 1) {
        for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
            if (it->workspaceID == WORKSPACEID) {
                it->isMaster = false;
                break;
            }
        }
    }

    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR   = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceID) {
        calculateWorkspace(PMONITOR->specialWorkspaceID);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL)
            return;

        // massive hack from the fullscreen func
        const auto      PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        SMasterNodeData fakeNode;
        fakeNode.pWindow         = PFULLWINDOW;
        fakeNode.position        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        fakeNode.size            = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        fakeNode.workspaceID     = PWORKSPACE->m_iID;
        PFULLWINDOW->m_vPosition = fakeNode.position;
        PFULLWINDOW->m_vSize     = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE->m_iID);
}

void CHyprMasterLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);

    if (!PWORKSPACE)
        return;

    const auto PWORKSPACEDATA = getMasterWorkspaceData(ws);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);

    const auto PMASTERNODE = getMasterNodeOnWorkspace(PWORKSPACE->m_iID);

    if (!PMASTERNODE)
        return;

    const auto MASTERS = getMastersOnWorkspace(PWORKSPACE->m_iID);

    //compute placement of master window(s)
    if (getNodesOnWorkspace(PWORKSPACE->m_iID) < 2) {
        PMASTERNODE->position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        PMASTERNODE->size     = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x,
                                     PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        applyNodeDataToWindow(PMASTERNODE);
        return;
    } else if (PWORKSPACEDATA->orientation == ORIENTATION_LEFT || PWORKSPACEDATA->orientation == ORIENTATION_RIGHT) {
        float       heightLeft = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y;
        int         nodesLeft  = MASTERS;
        float       nextY      = 0;
        const float WIDTH      = (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster;

        for (auto& n : m_lMasterNodesData) {
            if (n.workspaceID == PWORKSPACE->m_iID && n.isMaster) {
                if (PWORKSPACEDATA->orientation == ORIENTATION_RIGHT) {
                    n.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(PMONITOR->vecSize.x - WIDTH, nextY);
                } else {
                    n.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0, nextY);
                }
                float HEIGHT = nodesLeft > 1 ? heightLeft / nodesLeft * n.percSize : heightLeft;
                if (HEIGHT > heightLeft * 0.9f && nodesLeft > 1)
                    HEIGHT = heightLeft * 0.9f;
                n.size = Vector2D(WIDTH, HEIGHT);

                nodesLeft--;
                heightLeft -= HEIGHT;
                nextY += HEIGHT;

                applyNodeDataToWindow(&n);
            }
        }
    } else if (PWORKSPACEDATA->orientation == ORIENTATION_TOP || PWORKSPACEDATA->orientation == ORIENTATION_BOTTOM) {
        float       widthLeft = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x;
        int         nodesLeft = MASTERS;
        float       nextX     = 0;
        const float HEIGHT    = (PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y) * PMASTERNODE->percMaster;

        for (auto& n : m_lMasterNodesData) {
            if (n.workspaceID == PWORKSPACE->m_iID && n.isMaster) {
                if (PWORKSPACEDATA->orientation == ORIENTATION_BOTTOM) {
                    n.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(nextX, PMONITOR->vecSize.y - HEIGHT - PMONITOR->vecReservedBottomRight.y);
                } else {
                    n.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(nextX, 0);
                }
                float WIDTH = nodesLeft > 1 ? widthLeft / nodesLeft * n.percSize : widthLeft;
                if (WIDTH > widthLeft * 0.9f && nodesLeft > 1)
                    WIDTH = widthLeft * 0.9f;
                n.size = Vector2D(WIDTH, HEIGHT);

                nodesLeft--;
                widthLeft -= WIDTH;
                nextX += WIDTH;

                applyNodeDataToWindow(&n);
            }
        }
    }

    //compute placement of slave window(s)
    int slavesLeft = getNodesOnWorkspace(PWORKSPACE->m_iID) - MASTERS;
    if (PWORKSPACEDATA->orientation == ORIENTATION_LEFT || PWORKSPACEDATA->orientation == ORIENTATION_RIGHT) {
        float       heightLeft = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y;
        float       nextY      = 0;
        const float WIDTH      = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x - PMASTERNODE->size.x;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                continue;

            if (PWORKSPACEDATA->orientation == ORIENTATION_LEFT) {
                nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition +
                    Vector2D(PMASTERNODE->percMaster * (PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x), nextY);
            } else {
                nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(0, nextY);
            }
            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;
            nd.size = Vector2D(WIDTH, HEIGHT);

            slavesLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;

            applyNodeDataToWindow(&nd);
        }
    } else if (PWORKSPACEDATA->orientation == ORIENTATION_TOP || PWORKSPACEDATA->orientation == ORIENTATION_BOTTOM) {
        float       widthLeft = PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x;
        float       nextX     = 0;
        const float HEIGHT    = PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y - PMASTERNODE->size.y;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                continue;
            if (PWORKSPACEDATA->orientation == ORIENTATION_TOP) {
                nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition +
                    Vector2D(nextX, PMASTERNODE->percMaster * (PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y));
            } else {
                nd.position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition + Vector2D(nextX, 0);
            }
            float WIDTH = slavesLeft > 1 ? widthLeft / slavesLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && slavesLeft > 1)
                WIDTH = widthLeft * 0.9f;
            nd.size = Vector2D(WIDTH, HEIGHT);

            slavesLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;

            applyNodeDataToWindow(&nd);
        }
    }
}

void CHyprMasterLayout::applyNodeDataToWindow(SMasterNodeData* pNode) {
    CMonitor* PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceID == pNode->workspaceID) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node %x (workspace ID: %i)!!", pNode, pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;
    const auto PGAPSIN     = &g_pConfigManager->getConfigValuePtr("general:gaps_in")->intValue;
    const auto PGAPSOUT    = &g_pConfigManager->getConfigValuePtr("general:gaps_out")->intValue;

    const auto PWINDOW = pNode->pWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW)) {
        Debug::log(ERR, "Node %x holding invalid window %x!!", pNode, PWINDOW);
        return;
    }

    static auto* const PNOGAPSWHENONLY = &g_pConfigManager->getConfigValuePtr("master:no_gaps_when_only")->intValue;

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    auto calcPos  = PWINDOW->m_vPosition + Vector2D(*PBORDERSIZE, *PBORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

    if (*PNOGAPSWHENONLY && !g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) &&
        (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1 ||
         (PWINDOW->m_bIsFullscreen && g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_efFullscreenMode == FULLSCREEN_MAXIMIZED))) {
        PWINDOW->m_vRealPosition = calcPos - Vector2D(*PBORDERSIZE, *PBORDERSIZE);
        PWINDOW->m_vRealSize     = calcSize + Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

        PWINDOW->updateWindowDecos();

        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.border   = false;
        PWINDOW->m_sSpecialRenderData.decorate = false;

        return;
    }

    PWINDOW->m_sSpecialRenderData.rounding = true;
    PWINDOW->m_sSpecialRenderData.border   = true;
    PWINDOW->m_sSpecialRenderData.decorate = true;

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? *PGAPSOUT : *PGAPSIN, DISPLAYTOP ? *PGAPSOUT : *PGAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? *PGAPSOUT : *PGAPSIN, DISPLAYBOTTOM ? *PGAPSOUT : *PGAPSIN);

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID)) {
        static auto* const PSCALEFACTOR = &g_pConfigManager->getConfigValuePtr("master:special_scale_factor")->floatValue;

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize     = calcSize * *PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * *PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize     = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (m_bForceWarps) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprMasterLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprMasterLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::max((PWINDOW->m_vRealSize.goalv() + pixResize).x, 20.0), std::max((PWINDOW->m_vRealSize.goalv() + pixResize).y, 20.0));
        PWINDOW->updateWindowDecos();
        return;
    }

    // get monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    if (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) < 2)
        return;

    m_bForceWarps = true;

    double delta = pixResize.x / PMONITOR->vecSize.x;

    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == PMONITOR->activeWorkspace)
            n.percMaster = std::clamp(n.percMaster + delta, 0.05, 0.95);
    }

    // check the up/down resize
    if (pixResize.y != 0) {
        if (PNODE->isMaster && getMastersOnWorkspace(PNODE->workspaceID) > 1) {
            // check master size
            const auto SIZEY = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) / getMastersOnWorkspace(PNODE->workspaceID);
            PNODE->percSize  = std::clamp(PNODE->percSize + pixResize.y / SIZEY, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) - getMastersOnWorkspace(PNODE->workspaceID)) > 1) {
            const auto SIZEY = (PMONITOR->vecSize.y - PMONITOR->vecReservedTopLeft.y - PMONITOR->vecReservedBottomRight.y) / getNodesOnWorkspace(PNODE->workspaceID);
            PNODE->percSize  = std::clamp(PNODE->percSize + pixResize.y / SIZEY, 0.05, 1.95);
        }
    }

    recalculateMonitor(PMONITOR->ID);

    m_bForceWarps = false;
}

void CHyprMasterLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen || g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID))
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

    g_pEventManager->postEvent(SHyprIPCEvent{"fullscreen", std::to_string((int)on)});

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vPosition;
            pWindow->m_vRealSize     = pWindow->m_vSize;
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        PWORKSPACE->m_efFullscreenMode = fullscreenMode;

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vPosition = pWindow->m_vRealPosition.vec();
            pWindow->m_vSize     = pWindow->m_vRealSize.vec();
        }

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SMasterNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.position    = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size        = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize     = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->moveWindowToTop(pWindow);

    recalculateMonitor(PMONITOR->ID);
}

void CHyprMasterLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->m_iMonitorID);
}

SWindowRenderLayoutHints CHyprMasterLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CHyprMasterLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const auto inheritFullscreen = prepareLoseFocus(pWindow);

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_iWorkspaceID, pWindow->m_iWorkspaceID);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->m_iMonitorID);
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->m_iMonitorID);

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    prepareNewFocus(pWindow2, inheritFullscreen);
}

void CHyprMasterLayout::alterSplitRatioBy(CWindow* pWindow, float ratio) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

    PMASTER->percMaster = std::clamp(PMASTER->percMaster + ratio, 0.05f, 0.95f);

    recalculateMonitor(pWindow->m_iMonitorID);
}

CWindow* CHyprMasterLayout::getNextWindow(CWindow* pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    if (next) {
        if (PNODE->isMaster) {
            // focus the first non master
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow != pWindow && n.workspaceID == pWindow->m_iWorkspaceID) {
                    return n.pWindow;
                }
            }
        } else {
            // focus next
            bool reached = false;
            bool found   = false;
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow == pWindow) {
                    reached = true;
                    continue;
                }

                if (n.workspaceID == pWindow->m_iWorkspaceID && reached) {
                    return n.pWindow;
                }
            }
            if (!found) {
                const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

                if (PMASTER)
                    return PMASTER->pWindow;
            }
        }
    } else {
        if (PNODE->isMaster) {
            // focus the last non master
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->pWindow != pWindow && it->workspaceID == pWindow->m_iWorkspaceID) {
                    return it->pWindow;
                }
            }
        } else {
            // focus previous
            bool reached = false;
            bool found   = false;
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->pWindow == pWindow) {
                    reached = true;
                    continue;
                }

                if (it->workspaceID == pWindow->m_iWorkspaceID && reached) {
                    return it->pWindow;
                }
            }
            if (!found) {
                const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

                if (PMASTER)
                    return PMASTER->pWindow;
            }
        }
    }

    return nullptr;
}

bool CHyprMasterLayout::prepareLoseFocus(CWindow* pWindow) {
    if (!pWindow)
        return false;

    //if the current window is fullscreen, make it normal again if we are about to lose focus
    if (pWindow->m_bIsFullscreen) {
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);
        static auto* const INHERIT = &g_pConfigManager->getConfigValuePtr("master:inherit_fullscreen")->intValue;
        return *INHERIT == 1;
    }

    return false;
}

void CHyprMasterLayout::prepareNewFocus(CWindow* pWindow, bool inheritFullscreen) {
    if (!pWindow)
        return;

    if (inheritFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, true, FULLSCREEN_MAXIMIZED);
}

std::any CHyprMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        if (!g_pCompositor->windowValidMapped(PWINDOWTOCHANGETO))
            return;

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->m_vRealPosition.goalv() + PWINDOWTOCHANGETO->m_vRealSize.goalv() / 2.f);
    };

    if (message == "swapwithmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        if (!isWindowTiled(PWINDOW))
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow != PWINDOW) {
            switchWindows(PWINDOW, PMASTER->pWindow);
            switchToWindow(PWINDOW);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchWindows(n.pWindow, PMASTER->pWindow);
                    switchToWindow(n.pWindow);
                    break;
                }
            }
        }

        return 0;
    } else if (message == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow != PWINDOW) {
            switchToWindow(PMASTER->pWindow);
            prepareNewFocus(PMASTER->pWindow, inheritFullscreen);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow);
                    prepareNewFocus(n.pWindow, inheritFullscreen);
                    break;
                }
            }
        }

        return 0;
    } else if (message == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true);
        switchToWindow(PNEXTWINDOW);
        prepareNewFocus(PNEXTWINDOW, inheritFullscreen);
    } else if (message == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PPREVWINDOW = getNextWindow(PWINDOW, false);
        switchToWindow(PPREVWINDOW);
        prepareNewFocus(PPREVWINDOW, inheritFullscreen);
    } else if (message == "swapnext") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (message == "swapprev") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("prev");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (message == "addmaster") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->m_iWorkspaceID);
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->m_iWorkspaceID);

        if (MASTERS + 2 > WINDOWS)
            return 0;

        prepareLoseFocus(header.pWindow);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == header.pWindow->m_iWorkspaceID && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (message == "removemaster") {

        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->m_iWorkspaceID);
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->m_iWorkspaceID);

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        prepareLoseFocus(header.pWindow);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->workspaceID == header.pWindow->m_iWorkspaceID && it->isMaster) {
                    it->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (message == "orientationleft" || message == "orientationright" || message == "orientationtop" || message == "orientationbottom") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

        if (message == "orientationleft")
            PWORKSPACEDATA->orientation = ORIENTATION_LEFT;
        else if (message == "orientationright")
            PWORKSPACEDATA->orientation = ORIENTATION_RIGHT;
        else if (message == "orientationtop")
            PWORKSPACEDATA->orientation = ORIENTATION_TOP;
        else if (message == "orientationbottom")
            PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (message == "orientationnext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

        if (PWORKSPACEDATA->orientation == ORIENTATION_BOTTOM) {
            PWORKSPACEDATA->orientation = ORIENTATION_LEFT;
        } else {
            PWORKSPACEDATA->orientation = (eOrientation)(PWORKSPACEDATA->orientation + 1);
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (message == "orientationprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

        if (PWORKSPACEDATA->orientation == ORIENTATION_LEFT) {
            PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;
        } else {
            PWORKSPACEDATA->orientation = (eOrientation)(PWORKSPACEDATA->orientation - 1);
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    }

    return 0;
}

void CHyprMasterLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprMasterLayout::onDisable() {
    m_lMasterNodesData.clear();
}

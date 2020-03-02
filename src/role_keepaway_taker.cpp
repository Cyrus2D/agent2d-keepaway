// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "role_keepaway_taker.h"


#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_clear_ball.h>

#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/body_go_to_point2010.h>

using namespace rcsc;

const std::string RoleKeepawayTaker::NAME("KeepawayTaker");

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
    rcss::RegHolder role = SoccerRole::creators().autoReg(&RoleKeepawayTaker::create,
                                                          RoleKeepawayTaker::NAME);
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleKeepawayTaker::execute(PlayerAgent *agent) {

    //////////////////////////////////////////////////////////////
    // play_on play
    if (agent->world().self().isKickable()) {
        // kick
        doKick(agent);
    } else {
        // positioning
        doMove(agent);
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayTaker::doKick(PlayerAgent *agent) {
    Body_ClearBall().execute(agent);
    agent->setNeckAction(new Neck_ScanField());
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayTaker::doMove(PlayerAgent *agent) {
    const WorldModel &wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    if (!wm.existKickableTeammate()
        && self_min <= mate_min + 1) {
        Vector2D face_point(0.0, 0.0);
        Body_Intercept(true, face_point).execute(agent);
        agent->setNeckAction(new Neck_TurnToBallOrScan());
        return;
    }

    std::vector<double> dists(4);
    for (double &dist : dists)
        dist = 1000;
    for (int i = 1; i <= 2; i++) {
        auto tm = wm.ourPlayer(i);
        if (tm == NULL || tm->unum() < 1 || tm->unum() == wm.self().unum())
            continue;

        for (int j = 0; j <= 3; j++) {
            auto opp = wm.theirPlayer(j);
            if (opp == NULL || opp->unum() < 1 || opp->pos().dist(wm.ball().pos()) < 3)
                continue;

            Line2D pass_line = Line2D(wm.ball().pos(), opp->pos());
            double dist = pass_line.dist(opp->pos());
            if (dist < dists[i])
                dists[i] = dist;
        }
    }

    double max_dist = -2000;
    uint unum = 0;
    for (int i = 1; i <= 3; i++) {
        if (i == wm.self().unum() || dists[i] > 100)
            continue;
        if (max_dist < dists[i]) {
            max_dist = dists[i];
            unum = i;
        }
    }
    if (unum == 0) {
        Vector2D face_point(0.0, 0.0);
        Body_Intercept(true, face_point).execute(agent);
        agent->setNeckAction(new Neck_TurnToBallOrScan());
        return;
    }

    Vector2D target = (wm.ball().pos() + wm.theirPlayer(unum)->pos()) / 2;
    Body_GoToPoint2010(target, 0.5, 100).execute(agent);
    agent->setNeckAction(new Neck_TurnToBall());
}

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

#include "role_keepaway_keeper.h"

#include "strategy.h"

#include "bhv_chain_action.h"

#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_turn_to_ball.h>

#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/action/body_smart_kick.h>

using namespace rcsc;

const std::string RoleKeepawayKeeper::NAME("KeepawayKeeper");

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
    rcss::RegHolder role = SoccerRole::creators().autoReg(&RoleKeepawayKeeper::create,
                                                          RoleKeepawayKeeper::NAME);
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleKeepawayKeeper::execute(PlayerAgent *agent) {

    bool kickable = agent->world().self().isKickable();
    if (agent->world().existKickableTeammate()
        && agent->world().teammatesFromBall().front()->distFromBall()
           < agent->world().ball().distFromSelf()) {
        kickable = false;
    }

    if (kickable) {
        doKick(agent);
    } else {
        doMove(agent);
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayKeeper::doKick(PlayerAgent *agent) {
    const WorldModel &wm = agent->world();

    std::vector<double> dists(4);
    for (double &dist : dists)
        dist = 1000;
    for (int i = 1; i <= 3; i++) {
        auto tm = wm.ourPlayer(i);
        if (tm == NULL || tm->unum() < 1 || tm->unum() == wm.self().unum())
            continue;

        Line2D pass_line = Line2D(wm.self().pos(), tm->pos());

        for (int j = 0; j <= 2; j++) {
            auto opp = wm.theirPlayer(j);
            if (opp == NULL || opp->unum() < 1)
                continue;

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
    if (unum == 0)
        return;
    Body_SmartKick(wm.ourPlayer(unum)->pos(), 2, 1, 3).execute(agent);
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayKeeper::doMove(PlayerAgent *agent) {
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

    std::vector<Vector2D *> home_poses(4);
    std::vector<Vector2D> positions;
    positions.push_back(Vector2D(-9, -9));
    positions.push_back(Vector2D(9, 9));
    positions.push_back(Vector2D(-9, 9));
    positions.push_back(Vector2D(9, -9));

    for (auto position: positions) {
        dlog.addText(Logger::BLOCK, "*****************position: (%.2f,%.2f))", position.x, position.y);
        double min_dist = 1000;
        uint unum = 0;
        for (uint i = 1; i <= 3; i++) {
            auto tm = wm.ourPlayer(i);
            if (tm == NULL || tm->unum() < 1)
                continue;

            dlog.addText(Logger::BLOCK, "--->tmunums: %d", i);

            double dist = position.dist(tm->pos());
            if (dist < min_dist) {
                unum = i;
                min_dist = dist;
            }
//            std::pair<int, Vector2D>* pair = new std::pair(unum)
        }
        dlog.addText(Logger::BLOCK, "unum: %d", unum);
        if (home_poses[unum] == NULL) {
            home_poses[unum] = new Vector2D(position);
        } else {
            if (home_poses[unum]->dist(wm.ourPlayer(unum)->pos()) > min_dist)
                home_poses[unum] = new Vector2D(position);
        }
    }
    for (int i = 1; i <= 3; i++)
        dlog.addText(Logger::BLOCK, "unum: %d, pos: (%2.f, %2.f)", i, &home_poses[i]);

    Vector2D home_pos = home_poses[wm.self().unum()] == NULL ? Vector2D(0, 0) : *home_poses[wm.self().unum()];
    if (!Body_GoToPoint(home_pos,
                        0.5,
                        ServerParam::i().maxDashPower()).execute(agent)) {
        Body_TurnToBall().execute(agent);
    }

    agent->setNeckAction(new Neck_TurnToBallOrScan());
}

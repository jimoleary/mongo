/**
*    Copyright (C) 2012 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#include "mongo/db/geo/s2common.h"

#include "mongo/db/geo/geoconstants.h"
#include "mongo/db/geo/geoparser.h"
#include "mongo/db/geo/s2.h"
#include "third_party/s2/s2cell.h"
#include "third_party/s2/s2regioncoverer.h"

namespace mongo {

    static string myitoa(int d) {
        stringstream ss;
        ss << d;
        return ss.str();
    }

    void S2SearchUtil::setCoverLimitsBasedOnArea(double area, S2RegionCoverer *coverer,
                                                 int coarsestIndexedLevel) {
        area = sqrt(area);
        coverer->set_min_level(min(coarsestIndexedLevel, 2 + S2::kAvgEdge.GetClosestLevel(area)));
        coverer->set_max_level(4 + coverer->min_level());
    }

    BSONObj S2SearchUtil::coverAsBSON(const vector<S2CellId> &cover, const string& field,
                                      const int coarsestIndexedLevel) {
        BSONObjBuilder queryBuilder;
        BSONObjBuilder inBuilder(queryBuilder.subobjStart(field));
        // To have an array where elements of that array are regexes, we have to do this.
        BSONObjBuilder inArrayBuilder(inBuilder.subarrayStart("$in"));
        // Sadly we must keep track of this ourselves.  Oh, BSONObjBuilder, you rascal!
        int arrayPos = 0;

        bool considerCoarser = false;

        // Look at the cells we cover and all cells that are within our covering and
        // finer.  Anything with our cover as a strict prefix is contained within the cover and
        // should be intersection tested.
        for (size_t i = 0; i < cover.size(); ++i) {
            // First argument is position in the array as a string.
            // Third argument is options to regex.
            inArrayBuilder.appendRegex(myitoa(arrayPos++), "^" + cover[i].toString(), "");
            // If any of our covers could be covered by something in the index, we have
            // to look at things coarser.
            considerCoarser = considerCoarser || (cover[i].level() > coarsestIndexedLevel);
        }

        if (considerCoarser) {
            // Look at the cells that cover us.  We want to look at every cell that
            // contains the covering we would index on if we were to insert the
            // query geometry.  We generate the would-index-with-this-covering and
            // find all the cells strictly containing the cells in that set, until we hit the
            // coarsest indexed cell.  We use $in, not a prefix match.  Why not prefix?  Because
            // we've already looked at everything finer or as fine as our initial covering.
            //
            // Say we have a fine point with cell id 212121, we go up one, get 21212, we don't
            // want to look at cells 21212[not-1] because we know they're not going to intersect
            // with 212121, but entries inserted with cell value 21212 (no trailing digits) may.
            // And we've already looked at points with the cell id 211111 from the regex search
            // created above, so we only want things where the value of the last digit is not
            // stored (and therefore could be 1).
            unordered_set<S2CellId> parents;
            for (size_t i = 0; i < cover.size(); ++i) {
                for (S2CellId id = cover[i].parent(); id.level() >= coarsestIndexedLevel;
                        id = id.parent()) {
                    parents.insert(id);
                }
            }

            for (unordered_set<S2CellId>::const_iterator it = parents.begin(); it != parents.end(); ++it) {
                inArrayBuilder.append(myitoa(arrayPos++), it->toString());
            }
        }

        inArrayBuilder.done();
        inBuilder.done();
        return queryBuilder.obj();
    }
}  // namespace mongo

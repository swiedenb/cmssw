#define EDM_ML_DEBUG

#include "MTDDiskSectorBuilderFromDet.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DataFormats/GeometryVector/interface/VectorUtil.h"
#include "DataFormats/GeometrySurface/interface/BoundingBox.h"

#include <iomanip>

using namespace std;

namespace {

  pair<DiskSectorBounds*, GlobalVector> computeBounds(const vector<const GeomDet*>& dets, const Plane& plane) {
    Surface::PositionType tmpPos = dets.front()->surface().position();

    float rmin(plane.toLocal(tmpPos).perp());
    float rmax(plane.toLocal(tmpPos).perp());
    float zmin(plane.toLocal(tmpPos).z());
    float zmax(plane.toLocal(tmpPos).z());
    float phimin(plane.toLocal(tmpPos).phi());
    float phimax(plane.toLocal(tmpPos).phi());

    for (vector<const GeomDet*>::const_iterator it = dets.begin(); it != dets.end(); it++) {
      vector<GlobalPoint> corners = BoundingBox().corners((*it)->specificSurface());

      for (vector<GlobalPoint>::const_iterator i = corners.begin(); i != corners.end(); i++) {
        float r = plane.toLocal(*i).perp();
        float z = plane.toLocal(*i).z();
        float phi = plane.toLocal(*i).phi();
        rmin = min(rmin, r);
        rmax = max(rmax, r);
        zmin = min(zmin, z);
        zmax = max(zmax, z);
        if (Geom::phiLess(phi, phimin))
          phimin = phi;
        if (Geom::phiLess(phimax, phi))
          phimax = phi;
      }
      // in addition to the corners we have to check the middle of the
      // det +/- length/2, since the min (max) radius for typical fw
      // dets is reached there

      float rdet = (*it)->position().perp();
      float height = (*it)->surface().bounds().width();
      rmin = min(rmin, rdet - height / 2.F);
      rmax = max(rmax, rdet + height / 2.F);
    }

    if (!Geom::phiLess(phimin, phimax))
      edm::LogError("MTDDetLayers") << " MTDDiskSectorBuilderFromDet : "
                                    << "Something went wrong with Phi Sorting !";
    float zPos = (zmax + zmin) / 2.;
    float phiWin = phimax - phimin;
    float phiPos = (phimax + phimin) / 2.;
    float rmed = (rmin + rmax) / 2.;
    if (phiWin < 0.) {
      if ((phimin < Geom::pi() / 2.) || (phimax > -Geom::pi() / 2.)) {
        edm::LogError("MTDDetLayers") << " something strange going on, please check " << phimin << " " << phimax << " "
                                      << phiWin;
      }
      //edm::LogInfo(MTDDetLayers) << " Wedge at pi: phi " << phimin << " " << phimax << " " << phiWin
      //	 << " " << 2.*Geom::pi()+phiWin << " " ;
      phiWin += 2. * Geom::pi();
      phiPos += Geom::pi();
    }

    LocalVector localPos(rmed * cos(phiPos), rmed * sin(phiPos), zPos);

#ifdef EDM_ML_DEBUG
    LogDebug("MTDDetLayers") << "localPos in computeBounds: " << std::fixed << std::setw(14) << localPos << "\n"
                             << "rmin:   " << std::setw(14) << rmin << "\n"
                             << "rmax:   " << std::setw(14) << rmax << "\n"
                             << "zmin:   " << std::setw(14) << zmin << "\n"
                             << "zmax:   " << std::setw(14) << zmax << "\n"
                             << "phiWin: " << std::setw(14) << phiWin;

    LocalVector lX(1, 0, 0);
    LocalVector lY(0, 1, 0);
    LocalVector lZ(0, 0, 1);
    LogDebug("MTDDetLayers") << "Local versors transformations: \n"
                             << std::fixed << "x = " << std::setw(14) << plane.toGlobal(lX) << "\n"
                             << "y = " << std::setw(14) << plane.toGlobal(lY) << "\n"
                             << "z = " << std::setw(14) << plane.toGlobal(lZ);
#endif

    return make_pair(new DiskSectorBounds(rmin, rmax, zmin, zmax, phiWin), plane.toGlobal(localPos));
  }

  Surface::RotationType computeRotation(const vector<const GeomDet*>& dets, const Surface::PositionType& meanPos) {
    const Plane& plane = dets.front()->surface();

    GlobalVector xAxis;
    GlobalVector yAxis;
    GlobalVector zAxis;

    GlobalVector planeXAxis = plane.toGlobal(LocalVector(1, 0, 0));
    const GlobalPoint& planePosition = plane.position();

    if (planePosition.x() * planeXAxis.x() + planePosition.y() * planeXAxis.y() > 0.) {
      yAxis = planeXAxis;
    } else {
      yAxis = -planeXAxis;
    }

    GlobalVector planeZAxis = plane.toGlobal(LocalVector(0, 0, 1));
    if (planeZAxis.z() * planePosition.z() > 0.) {
      zAxis = planeZAxis;
    } else {
      zAxis = -planeZAxis;
    }

    xAxis = yAxis.cross(zAxis);

    return Surface::RotationType(xAxis, yAxis);
  }

}  // namespace

BoundDiskSector* MTDDiskSectorBuilderFromDet::operator()(const vector<const GeomDet*>& dets) const {
  // find mean position
  typedef Surface::PositionType::BasicVectorType Vector;
  Vector posSum(0, 0, 0);
  for (vector<const GeomDet*>::const_iterator i = dets.begin(); i != dets.end(); i++) {
    posSum += (**i).surface().position().basicVector();
  }
  Surface::PositionType meanPos(0., 0., posSum.z() / float(dets.size()));

  // temporary plane - for the computation of bounds
  Surface::RotationType rotation = computeRotation(dets, meanPos);
  Plane tmpPlane(meanPos, rotation);

  auto bo = computeBounds(dets, tmpPlane);
  GlobalPoint pos = meanPos + bo.second;
  LogDebug("MTDDetLayers") << "global pos in operator: " << std::fixed << std::setw(14) << pos;
  return new BoundDiskSector(pos, rotation, bo.first);
}

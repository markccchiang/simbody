#ifndef SimTK_SIMBODY_MOBILIZED_BODY_REP_H_
#define SimTK_SIMBODY_MOBILIZED_BODY_REP_H_

/* Portions copyright (c) 2007 Stanford University and Michael Sherman.
 * Contributors:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**@file
 *
 * Private implementation of Body and MobilizedBody, and their included subclasses which
 * represent the built-in body and mobilizer types.
 */

#include "SimTKcommon.h"
#include "simbody/internal/common.h"
#include "simbody/internal/Body.h"
#include "simbody/internal/MobilizedBody.h"

#include "SimbodyMatterSubsystemRep.h"
#include "BodyRep.h"

class RigidBodyNode;

namespace SimTK {

    /////////////////////////
    // MOBILIZED BODY REPS //
    /////////////////////////

class MobilizedBody::MobilizedBodyRep {
public:
    MobilizedBodyRep() : myHandle(0), myMatterSubsystemRep(0), myRBnode(0) {
    }
    virtual ~MobilizedBodyRep() {
        delete myRBnode;
    }
    virtual MobilizedBodyRep* clone() const = 0;
    
    // This creates a rigid body node using the appropriate mobilizer type.
    // Called during realizeTopology().
    virtual RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const = 0; 

    // Copy out nq default values for q, beginning at the indicated address.
    // The concrete class should assert if nq is not a reasonable
    // number for the kind of mobilizer; there is a bug somewhere in that case. 
    // This routine shouldn't be called directly -- call copyOutDefaultQ() below 
    // instead which has a nicer interface and does some error checking.
    virtual void copyOutDefaultQImpl(int nq, Real* q) const = 0;

    void addOutboardDecoration(const Transform& X_MD, const DecorativeGeometry& g) {
        outboardGeometry.push_back(g); // make a new copy
        // Combine the placement frame and the transform already in the geometry
        // so we end up with geometry expressed directly in the M frame.
        outboardGeometry.back().setTransform(X_MD*g.getTransform());
    }
    void addInboardDecoration(const Transform& X_MbD, const DecorativeGeometry& g) {
        inboardGeometry.push_back(g); // make a new copy
        // Combine the placement frame and the transform already in the geometry
        // so we end up with geometry expressed directly in the Mb frame.
        inboardGeometry.back().setTransform(X_MbD*g.getTransform());
    }

    void appendBodyGeometry(Array<DecorativeGeometry>& geom) const {
        getBody().getRep().appendDecorativeGeometry(getMyMobilizedBodyId(), geom);
    }

    // TODO: should call at Instance stage to wait for M and Mb instantiation.
    void appendMobilizerGeometry(const Transform& X_BM, const Transform& X_PMb,
                                 Array<DecorativeGeometry>& geom) const
    {
        for (int i=0; i<(int)outboardGeometry.size(); ++i) {
            geom.push_back(outboardGeometry[i]);
            geom.back().setBodyId(getMyMobilizedBodyId())
                       .setTransform(X_BM*outboardGeometry[i].getTransform());
        }
        for (int i=0; i<(int)inboardGeometry.size(); ++i) {
            geom.push_back(inboardGeometry[i]);
            geom.back().setBodyId(getMyParentMobilizedBodyId())
                       .setTransform(X_PMb*inboardGeometry[i].getTransform());
        }
    }

    int getQIndex(const State&) const;
    int getUIndex(const State&) const;

    // Given the Model stage state variables (if any are relevant), each
    // concrete mobilizer should return the number of q's and u's it expects
    // to be allotted.
    //virtual void getStateNeeds(const State& s, int& nq, int& nu) const = 0;

    // Given the Model stage variable values in the passed-in State (if
    // any of them are relevant) copy out the appropriate default values
    // to the appropriate slot in qDefault.
    void copyOutDefaultQ(const State& s, Vector& qDefault) const;

    // Generic State-access routines (that is, those which can be handled in
    // the MobilizedBody base class).


    // Invalidate Stage::Position.
    void setQToFitTransform(State& s, const Transform& X_MbM) const;
    void setQToFitRotation(State& s, const Rotation& R_MbM) const;
    void setQToFitTranslation(State& s, const Vec3& T_MbM, 
                                 bool dontChangeOrientation) const;

    // Invalidate Stage::Velocity.
    void setUToFitVelocity(State& s, const SpatialVec& V_MbM) const;
    void setUToFitAngularVelocity(State& s, const Vec3& w_MbM) const;
    void setUToFitLinearVelocity(State& s, const Vec3& v_MbM,
                                 bool dontChangeAngularVelocity)  const;

    const Transform& getBodyTransform(const State& s) const {
        const SBPositionCache& pc = getMyMatterSubsystemRep().getPositionCache(s);
        return getMyRigidBodyNode().getX_GB(pc);
    }
    const SpatialVec& getBodyVelocity(const State& s) const {
        const SBVelocityCache& vc = getMyMatterSubsystemRep().getVelocityCache(s);
        return getMyRigidBodyNode().getV_GB(vc);
    }
    const SpatialVec& getBodyAcceleration(const State& s) const {
        const SBAccelerationCache& ac = getMyMatterSubsystemRep().getAccelerationCache(s);
        return getMyRigidBodyNode().getA_GB(ac);
    }

    const Transform& getMobilizerTransform(const State& s) const {
        const SBPositionCache& pc = getMyMatterSubsystemRep().getPositionCache(s);
        return getMyRigidBodyNode().getX_MbM(pc);
    }
    const SpatialVec& getMobilizerVelocity(const State& s) const {
        const SBVelocityCache& vc = getMyMatterSubsystemRep().getVelocityCache(s);
        return getMyRigidBodyNode().getV_MbM(vc);
    }

    const RigidBodyNode& realizeTopology(int& nxtU, int& nxtUSq, int& nxtQ) const;

    void invalidateTopologyCache() const {
        delete myRBnode; myRBnode=0;
        if (myMatterSubsystemRep)
            myMatterSubsystemRep->invalidateSubsystemTopologyCache();
    }

    const RigidBodyNode& getMyRigidBodyNode() const {
        SimTK_ASSERT(myRBnode && myMatterSubsystemRep && 
                     myMatterSubsystemRep->subsystemTopologyHasBeenRealized(),
          "An operation on a MobilizedBody was illegal because realizeTopology() has "
          "not been performed on the containing Subsystem since the last topological change."
        );
        return *myRBnode;
    }

    const Body& getBody() const {return theBody;}
    const Transform& getDefaultInboardFrame()  const {return defaultInboardFrame;}
    const Transform& getDefaultOutboardFrame() const {return defaultOutboardFrame;}
    const MassProperties& getDefaultRigidBodyMassProperties() const {
        return theBody.getDefaultRigidBodyMassProperties();
    }

    const SimbodyMatterSubsystemRep& getMyMatterSubsystemRep() const {
        SimTK_ASSERT(myMatterSubsystemRep,
            "An operation was illegal because a MobilizedBody was not in a Subsystem.");
        return *myMatterSubsystemRep;
    }
    SimbodyMatterSubsystemRep& updMyMatterSubsystemRep() {
        SimTK_ASSERT(myMatterSubsystemRep,
            "An operation was illegal because a MobilizedBody was not in a Subsystem.");
        return *myMatterSubsystemRep;
    }

    MobilizedBodyId getMyMobilizedBodyId() const {
        assert(myMobilizedBodyId.isValid());
        return myMobilizedBodyId;
    }

    MobilizedBodyId getMyParentMobilizedBodyId() const {
        assert(myParentId.isValid());
        return myParentId;
    }

    bool isInSubsystem() const {return myMatterSubsystemRep != 0;}

    void setMyMatterSubsystem(SimbodyMatterSubsystem& matter,
                              MobilizedBodyId  parentId,
                              MobilizedBodyId  id)
    {
        assert(!myMatterSubsystemRep);
        myMatterSubsystemRep = &matter.updRep();
        myParentId           = parentId;
        myMobilizedBodyId    = id;
    }

    void setMyHandle(MobilizedBody& h) {myHandle = &h;}
    const MobilizedBody& getMyHandle() const {assert(myHandle); return *myHandle;}
    void clearMyHandle() {myHandle=0;}

private:
    friend class MobilizedBody;
    MobilizedBody* myHandle;	// the owner handle of this rep

        // TOPOLOGY "STATE"

    // Base class topological properties. Derived MobilizedBodies may
    // have further topological properties. Whenever these change, be
    // sure to set topologyRealized=false!
    Body theBody;
    Transform defaultInboardFrame;  // default for Mb (in Parent frame)
    Transform defaultOutboardFrame; // default for M (in Body frame)

    std::vector<DecorativeGeometry> outboardGeometry;
    std::vector<DecorativeGeometry> inboardGeometry;

    // These data members are filled in once the MobilizedBody is added to
    // a MatterSubsystem. Note that this pointer is just a reference to
    // the Subsystem which owns this MobilizedBody -- we don't own the
    // heap space here so don't delete it!
    SimbodyMatterSubsystemRep* myMatterSubsystemRep;
    MobilizedBodyId  myMobilizedBodyId; // id within the subsystem
    MobilizedBodyId  myParentId;

        // TOPOLOGY "CACHE"

    // A RigidBodyNode is created for each MobilizedBody during realizeTopology().
    // Think of it as the *computational* form of the MobilizedBody; the MobilizedBody
    // itself exists to make a nice API for the programmer. In fact is is possible
    // for several different MobilizedBody classes to use the same underlying
    // computational form while providing a different API.
    //
    // This is a pointer to an abstract object whose heap space is *owned* by
    // this MobilizedBody. Be sure to delete it upon destruction and whenever
    // topology is re-realized.
    mutable RigidBodyNode* myRBnode;
};

class MobilizedBody::Pin::PinRep : public MobilizedBody::MobilizedBodyRep {
public:
    PinRep() : defaultQ(0) { }
    PinRep* clone() const { return new PinRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==1, 
            "MobilizedBody::Pin::PinRep::copyOutDefaultQImpl(): wrong number of q's expected");
        *q = defaultQ;
    }

    SimTK_DOWNCAST(PinRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Pin;
    Real defaultQ;  // the angle in radians
};


class MobilizedBody::Slider::SliderRep : public MobilizedBody::MobilizedBodyRep {
public:
    SliderRep() : defaultQ(0) { }
    SliderRep* clone() const { return new SliderRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==1, 
            "MobilizedBody::Slider::SliderRep::copyOutDefaultQImpl(): wrong number of q's expected");
        *q = defaultQ;
    }

    SimTK_DOWNCAST(SliderRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Slider;
    Real defaultQ;  // the displacement
};

class MobilizedBody::Universal::UniversalRep : public MobilizedBody::MobilizedBodyRep {
public:
    UniversalRep() : defaultQ(0) { }
    UniversalRep* clone() const { return new UniversalRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==2, 
            "MobilizedBody::Universal::UniversalRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec2::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(UniversalRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Universal;
    Vec2 defaultQ;  // the two angles
};

class MobilizedBody::Cylinder::CylinderRep : public MobilizedBody::MobilizedBodyRep {
public:
    CylinderRep() : defaultQ(0) { }
    CylinderRep* clone() const { return new CylinderRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==2, 
            "MobilizedBody::Cylinder::CylinderRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec2::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(CylinderRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Cylinder;
    Vec2 defaultQ;  // the angle in radians followed by the displacement
};

class MobilizedBody::BendStretch::BendStretchRep : public MobilizedBody::MobilizedBodyRep {
public:
    BendStretchRep() : defaultQ(0) { }
    BendStretchRep* clone() const { return new BendStretchRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==2, 
            "MobilizedBody::BendStretch::BendStretchRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec2::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(BendStretchRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::BendStretch;
    Vec2 defaultQ;  // the angle in radians followed by the displacement
};

class MobilizedBody::Planar::PlanarRep : public MobilizedBody::MobilizedBodyRep {
public:
    PlanarRep() : defaultQ(0) { }
    PlanarRep* clone() const { return new PlanarRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==3, 
            "MobilizedBody::Planar::PlanarRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec3::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(PlanarRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Planar;
    Vec3 defaultQ;  // the angle in radians followed by the two displacements
};

class MobilizedBody::Gimbal::GimbalRep : public MobilizedBody::MobilizedBodyRep {
public:
    GimbalRep() : defaultQ(0) { }
    GimbalRep* clone() const { return new GimbalRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==3, 
            "MobilizedBody::Gimbal::GimbalRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec3::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(GimbalRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Gimbal;
    Vec3 defaultQ;  // the three angles in radians
};

class MobilizedBody::Ball::BallRep : public MobilizedBody::MobilizedBodyRep {
public:
    BallRep() : defaultRadius(0.5), defaultQ() { } // default is (1,0,0,0), the identity rotation
    BallRep* clone() const { return new BallRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==4||nq==3, 
            "MobilizedBody::Ball::BallRep::copyOutDefaultQImpl(): wrong number of q's expected");
        if (nq==4)
            Vec4::updAs(q) = defaultQ.asVec4();
        else
            Vec3::updAs(q) = Rotation(defaultQ).convertToBodyFixed123();
    }

    void setDefaultRadius(Real r) {
        assert(r>0);
        defaultRadius=r;
    }
    Real getDefaultRadius() const {return defaultRadius;}

    SimTK_DOWNCAST(BallRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Ball;
    Real defaultRadius;   // used for visualization only
    Quaternion defaultQ;  // the default orientation
};

class MobilizedBody::Ellipsoid::EllipsoidRep : public MobilizedBody::MobilizedBodyRep {
public:
    EllipsoidRep() : defaultRadii(0.5,1/3.,0.25), defaultQ() { } // default is (1,0,0,0), the identity rotation
    EllipsoidRep* clone() const { return new EllipsoidRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==4||nq==3, 
            "MobilizedBody::Ellipsoid::EllipsoidRep::copyOutDefaultQImpl(): wrong number of q's expected");
        if (nq==4)
            Vec4::updAs(q) = defaultQ.asVec4();
        else
            Vec3::updAs(q) = Rotation(defaultQ).convertToBodyFixed123();
    }

    void setDefaultRadii(const Vec3& r) {
        assert(r[0]>0 && r[1]>0 && r[2]>0);
        defaultRadii=r;
    }
    const Vec3& getDefaultRadii() const {return defaultRadii;}

    SimTK_DOWNCAST(EllipsoidRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Ellipsoid;
    Vec3 defaultRadii;    // used for visualization only
    Quaternion defaultQ;  // the default orientation
};

class MobilizedBody::Translation::TranslationRep : public MobilizedBody::MobilizedBodyRep {
public:
    TranslationRep() : defaultQ(0) { }
    TranslationRep* clone() const { return new TranslationRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==3, 
            "MobilizedBody::Translation::TranslationRep::copyOutDefaultQImpl(): wrong number of q's expected");
        Vec3::updAs(q) = defaultQ;
    }

    SimTK_DOWNCAST(TranslationRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Translation;
    Vec3 defaultQ;  // the three displacements
};

class MobilizedBody::Free::FreeRep : public MobilizedBody::MobilizedBodyRep {
public:
    FreeRep() : defaultQOrientation(), defaultQTranslation(0) { }
    FreeRep* clone() const { return new FreeRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==7||nq==6, 
            "MobilizedBody::Free::FreeRep::copyOutDefaultQImpl(): wrong number of q's expected");
        if (nq==7) {
            Vec4::updAs(q)   = defaultQOrientation.asVec4();
            Vec3::updAs(q+4) = defaultQTranslation;
        } else {
            Vec3::updAs(q)   = Rotation(defaultQOrientation).convertToBodyFixed123();
            Vec3::updAs(q+3) = defaultQTranslation;
        }
    }

    SimTK_DOWNCAST(FreeRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Free;
    Quaternion defaultQOrientation;
    Vec3       defaultQTranslation;
};

class MobilizedBody::LineOrientation::LineOrientationRep : public MobilizedBody::MobilizedBodyRep {
public:
    LineOrientationRep() : defaultQ() { } // 1,0,0,0
    LineOrientationRep* clone() const { return new LineOrientationRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==4||nq==3, 
            "MobilizedBody::LineOrientation::LineOrientationRep::copyOutDefaultQImpl(): wrong number of q's expected");
        if (nq==4)
            Vec4::updAs(q) = defaultQ.asVec4();
        else
            Vec3::updAs(q) = Rotation(defaultQ).convertToBodyFixed123();
    }

    SimTK_DOWNCAST(LineOrientationRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::LineOrientation;
    Quaternion defaultQ;
};

class MobilizedBody::FreeLine::FreeLineRep : public MobilizedBody::MobilizedBodyRep {
public:
    FreeLineRep() : defaultQOrientation(), defaultQTranslation(0) { }
    FreeLineRep* clone() const { return new FreeLineRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==7||nq==6, 
            "MobilizedBody::FreeLine::FreeLineRep::copyOutDefaultQImpl(): wrong number of q's expected");
        if (nq==7) {
            Vec4::updAs(q)   = defaultQOrientation.asVec4();
            Vec3::updAs(q+4) = defaultQTranslation;
        } else {
            Vec3::updAs(q)   = Rotation(defaultQOrientation).convertToBodyFixed123();
            Vec3::updAs(q+3) = defaultQTranslation;
        }
    }

    SimTK_DOWNCAST(FreeLineRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::FreeLine;
    Quaternion defaultQOrientation;
    Vec3       defaultQTranslation;
};

class MobilizedBody::Weld::WeldRep : public MobilizedBody::MobilizedBodyRep {
public:
    WeldRep() { }
    WeldRep* clone() const { return new WeldRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==0, 
            "MobilizedBody::Weld::WeldRep::copyOutDefaultQImpl(): wrong number of q's expected");
    }

    SimTK_DOWNCAST(WeldRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Weld;
    // no q's
};

class MobilizedBody::Ground::GroundRep : public MobilizedBody::MobilizedBodyRep {
public:
    GroundRep() { }
    GroundRep* clone() const { return new GroundRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==0, 
            "MobilizedBody::Ground::GroundRep::copyOutDefaultQImpl(): wrong number of q's expected");
    }

    SimTK_DOWNCAST(GroundRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Ground;
    // no q's
};

class MobilizedBody::Screw::ScrewRep : public MobilizedBody::MobilizedBodyRep {
public:
    explicit ScrewRep(Real p) : defaultPitch(p), defaultQ(0) { }

    Real getDefaultPitch() const {return defaultPitch;}
    void setDefaultPitch(Real p) {defaultPitch=p;}

    ScrewRep* clone() const { return new ScrewRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int nq, Real* q) const {
        SimTK_ASSERT(nq==1, 
            "MobilizedBody::Screw::ScrewRep::copyOutDefaultQImpl(): wrong number of q's expected");
        *q = defaultQ;
    }

    SimTK_DOWNCAST(ScrewRep, MobilizedBodyRep);
private:
    friend class MobilizedBody::Screw;
    Real defaultPitch;
    Real defaultQ; // the angle in radians
};

class MobilizedBody::Custom::CustomRep : public MobilizedBody::MobilizedBodyRep {
public:
    CustomRep(int nMobilities, int nCoordinates) 
      : nu(nMobilities), nq(nCoordinates), defaultQ(nq) {
        defaultQ.setToZero();
    }
    CustomRep* clone() const { return new CustomRep(*this); }

    RigidBodyNode* createRigidBodyNode(
        int&                     nxtU,
        int&                     nxtUSq,
        int&                     nxtQ) const;

    void copyOutDefaultQImpl(int expectedNq, Real* q) const {
        SimTK_ASSERT(expectedNq==nq, 
            "MobilizedBody::Custom::CustomRep::copyOutDefaultQImpl(): wrong number of q's expected");
        for (int i=0; i<nq; ++i)
            q[i] = defaultQ[i];
    }

    SimTK_DOWNCAST(CustomRep, MobilizedBodyRep);
private:
    int nu;
    int nq;
    Vector defaultQ;
};

} // namespace SimTK

#endif // SimTK_SIMBODY_MOBILIZED_BODY_REP_H_
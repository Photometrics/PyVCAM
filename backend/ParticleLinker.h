/******************************************************************************/
/* Copyright by Photometrics and QImaging. All rights reserved.               */
/******************************************************************************/
#pragma once
#ifndef PM_PARTICLE_LINKER_H
#define PM_PARTICLE_LINKER_H

/* Local */
#include "Frame.h"
#include "PrdFileFormat.h"

/* System */
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

// Forward declaration from pvcam_helper_track.h
struct ph_track_particle;

namespace pm {

class ParticleLinker
{
public:
    ParticleLinker(uint32_t maxTrajectories, uint32_t maxTrajectoryPoints);

public:
    void AddParticles(const ph_track_particle* pParticles, uint32_t count);
    const Frame::Trajectories& GetTrajectories() const;

private:
    struct Queues
    {
        uint16_t currentRoiNr;
        uint32_t validPoints;
        std::deque<PrdTrajectoryPoint> points;
    };

private:
    uint32_t m_depth;
    std::map<uint32_t, Queues> m_particles; // Key is particle id
    Frame::Trajectories m_trajectories;
};

} // namespace pm

#endif /* PM_PARTICLE_LINKER_H */

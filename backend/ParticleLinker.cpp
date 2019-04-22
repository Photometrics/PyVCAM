#include "ParticleLinker.h"

/* pvcam_helper_track */
#include <pvcam_helper_track.h>

/* System */
#include <algorithm> // std::min

pm::ParticleLinker::ParticleLinker(uint32_t maxTrajectories,
        uint32_t maxTrajectoryPoints)
    : m_depth(maxTrajectoryPoints),
    m_particles(),
    m_trajectories()
{
    m_trajectories.header.maxTrajectories = maxTrajectories;
    m_trajectories.header.maxTrajectoryPoints = maxTrajectoryPoints;
}

void pm::ParticleLinker::AddParticles(const ph_track_particle* pParticles,
        uint32_t count)
{
    // Key is particle id
    std::map<uint32_t, const ph_track_particle*> newParticles;

    for (uint32_t n = 0; n < count; n++)
    {
        const ph_track_particle* particle = &pParticles[n];

        // Build lookup map
        newParticles[particle->id] = particle;

        // Add records for new particles
        if (m_particles.count(particle->id) == 0)
        {
            // Not found - create empty record
            Queues queues;
            // Queue for particle points with max count of invalid points
            queues.points = std::deque<PrdTrajectoryPoint>(m_depth, { 0, 0, 0 });
            // Set counter for valid values
            queues.validPoints = 0;
            // Create entry in the map
            m_particles[particle->id] = queues;
        }

        // Set/update ROI number to match current frame
        m_particles[particle->id].currentRoiNr = particle->event.roiNr;
    }

    // m_particles now contains data from both current and previous frames,
    // for each particle we shift one new item into queue and drop one item out
    for (auto it = m_particles.begin(); it != m_particles.end(); )
    {
        const uint32_t particleId = it->first;
        auto& queues = it->second;

        PrdTrajectoryPoint newPoint;
        if (newParticles.count(particleId) == 0)
        {
            // Particle not found on current frame, use a placeholder
            newPoint.isValid = 0; // false
            newPoint.x = 0;
            newPoint.y = 0;

            // Reset ROI number to be invalid
            queues.currentRoiNr = 0;
        }
        else
        {
            // Particle found on current frame thus it is valid
            newPoint.isValid = 1; // true
            const auto particle = newParticles[particleId];
            newPoint.x = (uint16_t)particle->event.center.x;
            newPoint.y = (uint16_t)particle->event.center.y;
        }

        // Push new to front
        queues.points.push_front(newPoint);
        if (newPoint.isValid)
        {
            queues.validPoints++;
        }

        // Throw away the oldest from back
        // (There is for sure m_depth+1 elements in the queue at the moment)
        const auto& oldPoint = queues.points[m_depth];
        if (oldPoint.isValid)
        {
            queues.validPoints--;
        }
        queues.points.pop_back();

        // When counter is 0, particle was not found in whole history depth
        if (queues.validPoints == 0)
        {
            it = m_particles.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Update trajectories
    m_trajectories.data.clear();
    // Going through particles
    for (auto& particlePair : m_particles)
    {
        const auto& particleId = particlePair.first;
        const auto& queues = particlePair.second;

        // Skip particle missing in current frame
        if (newParticles.count(particleId) == 0)
            continue;

        Frame::Trajectory trajectory;
        trajectory.header.roiNr = queues.currentRoiNr;
        trajectory.header.particleId = particleId;
        trajectory.header.lifetime = newParticles[particleId]->lifetime;
        trajectory.data.assign(queues.points.begin(), queues.points.end());
        trajectory.data.resize(std::min<size_t>(
                trajectory.data.size(),
                trajectory.header.lifetime));
        trajectory.header.pointCount =
            static_cast<uint32_t>(trajectory.data.size());

        m_trajectories.data.push_back(trajectory);
    }
    m_trajectories.header.trajectoryCount =
        static_cast<uint32_t>(m_trajectories.data.size());
}

const pm::Frame::Trajectories& pm::ParticleLinker::GetTrajectories() const
{
    return m_trajectories;
}

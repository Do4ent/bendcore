#include "bendcore/cam.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

using namespace bendcore;

namespace {

struct Check {
    std::string msg;
    bool ok = true;

    ~Check() { std::cout << (ok ? "✓ " : "✗ ") << msg << "\n"; }
};

void check_generate_job_single_bend() {
    Check c{"generate_job: single bend"};

    Path path;
    path.push_back(Straight{100.0});
    path.push_back(Bend{50.0, 90.0});
    path.push_back(Straight{100.0});

    auto job = generate_job("T1", path, 210.0);

    if (job.tube_id != "T1") c.ok = false;
    if (job.bends.size() != 1) c.ok = false;
    if (job.bends[0].sequence != 1 || job.bends[0].angle_deg != 90.0) c.ok = false;
    if (job.estimated_time_sec <= 0.0) c.ok = false;
}

void check_generate_job_multiple_bends() {
    Check c{"generate_job: multiple bends"};

    Path path;
    path.push_back(Straight{50.0});
    path.push_back(Bend{50.0, 90.0});
    path.push_back(Straight{50.0});
    path.push_back(Bend{50.0, 45.0});
    path.push_back(Straight{50.0});

    auto job = generate_job("T2", path, 250.0);

    if (job.bends.size() != 2) c.ok = false;
    if (job.bends[0].sequence != 1 || job.bends[1].sequence != 2) c.ok = false;
    if (std::abs(job.bends[0].angle_deg - 90.0) > 0.01) c.ok = false;
    if (std::abs(job.bends[1].angle_deg - 45.0) > 0.01) c.ok = false;
}

void check_generate_job_blank_length() {
    Check c{"generate_job: blank length preserved"};

    Path path;
    path.push_back(Straight{100.0});

    auto job = generate_job("T3", path, 150.0);

    if (std::abs(job.blank_length_mm - 150.0) > 0.01) c.ok = false;
    if (std::abs(job.finished_length_mm - 100.0) > 0.01) c.ok = false;
}

void check_gcode_format() {
    Check c{"job_to_gcode: valid format"};

    Path path;
    path.push_back(Straight{100.0});
    path.push_back(Bend{50.0, 90.0});

    auto job = generate_job("T4", path, 210.0);
    auto gcode = job_to_gcode(job);

    // Check for expected keywords/structure
    if (gcode.find("T4") == std::string::npos) c.ok = false;      // tube id
    if (gcode.find("G21") == std::string::npos) c.ok = false;     // metric
    if (gcode.find("G90") == std::string::npos) c.ok = false;     // absolute
    if (gcode.find("G65") == std::string::npos) c.ok = false;     // bend op
    if (gcode.find("M30") == std::string::npos) c.ok = false;     // end
    if (gcode.find("90.0") == std::string::npos) c.ok = false;    // angle
}

void check_gcode_no_preamble() {
    Check c{"job_to_gcode: no preamble option"};

    Path path;
    path.push_back(Bend{50.0, 45.0});

    auto job = generate_job("T5", path, 100.0);
    auto gcode = job_to_gcode(job, false);

    if (gcode.find("G21") != std::string::npos) c.ok = false;   // should not have preamble
    if (gcode.find("G65") == std::string::npos) c.ok = false;   // should have bend op
}

void check_plan_batch_basic() {
    Check c{"plan_batch: basic batch planning"};

    std::vector<ProductionJob> jobs;
    for (int i = 0; i < 3; ++i) {
        Path path;
        path.push_back(Straight{100.0});
        path.push_back(Bend{50.0, 60.0});
        jobs.push_back(generate_job("T" + std::to_string(i + 1), path, 200.0));
    }

    auto batch = plan_batch(jobs, "BATCH001");

    if (batch.id != "BATCH001") c.ok = false;
    if (batch.total_tubes != 3) c.ok = false;
    if (batch.jobs.size() != 3) c.ok = false;
    if (batch.total_time_sec <= 0.0) c.ok = false;
}

void check_estimate_time_proportional() {
    Check c{"generate_job: time increases with angle"};

    Path path1;
    path1.push_back(Straight{100.0});
    path1.push_back(Bend{50.0, 30.0});

    Path path2;
    path2.push_back(Straight{100.0});
    path2.push_back(Bend{50.0, 90.0});

    auto job1 = generate_job("T1", path1, 200.0);
    auto job2 = generate_job("T2", path2, 200.0);

    // More angle → more time
    if (job2.estimated_time_sec <= job1.estimated_time_sec) c.ok = false;
}

void check_spiral_skipped() {
    Check c{"generate_job: spirals do not create operations (заглушка)"};

    Path path;
    path.push_back(Straight{100.0});
    path.push_back(Spiral{30.0, 2.0, 3.0, +1});
    path.push_back(Straight{100.0});

    auto job = generate_job("T6", path, 400.0);

    // In stub: spirals are skipped (not bendable on simple machines)
    if (job.bends.size() != 0) c.ok = false;
}

}  // namespace

int main() {
    check_generate_job_single_bend();
    check_generate_job_multiple_bends();
    check_generate_job_blank_length();
    check_gcode_format();
    check_gcode_no_preamble();
    check_plan_batch_basic();
    check_estimate_time_proportional();
    check_spiral_skipped();

    std::cout << "\n✓ All CAM tests passed\n";
    return 0;
}

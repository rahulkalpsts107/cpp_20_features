#include <benchmark/benchmark.h>
#include <cmath>
#include <string>
#include <iostream>
#include <vector>
#include <numeric>  // Add this for std::accumulate
#include <variant>  // Add this include
#include <concepts>

// Constants
namespace constants {
    static const size_t VECTOR_SIZE = 10000;
    static const int INNER_LOOP_COUNT = 10;
    static const int BENCHMARK_ITERATIONS = 10000;  // Changed from 5000 to 10000
    static const double ACCELERATION_VALUE = 50.0;
    static const double MIN_TIME = 0.1;
    static const double POLY_ACCELERATION = 0.8;
    static const double POLY_FRICTION = 0.9;
    static const double ELECTRIC_ACCELERATION = 0.95;  // Higher acceleration for electric
    static const double ELECTRIC_FRICTION = 0.85;      // Lower friction for electric
    static const double GAS_ACCELERATION = 0.75;       // Lower acceleration for gas
    static const double GAS_FRICTION = 0.95;           // Higher friction for gas
}

// Common implementation namespace
namespace impl {
    inline void doAcceleration(const double speed, double& work, std::vector<double>& calculations) {
        for(int i = 0; i < constants::VECTOR_SIZE; ++i) {
            calculations[i] = std::sin(speed * i) * std::cos(i * 0.5) * std::tan(speed);
            work += calculations[i] * std::sqrt(speed + i);
        }
    }

    inline void reportStats(benchmark::State& state, const double total_work) {
        state.counters["iterations"] = state.iterations();
        state.counters["items_per_second"] = benchmark::Counter(
            state.iterations(), benchmark::Counter::kIsRate);
        state.counters["avg_work_per_iteration"] = total_work / state.iterations();
        state.SetLabel(std::string("Total accumulated work (from calculations): ") + 
                      std::to_string(total_work) + 
                      " (avg per iter: " + 
                      std::to_string(total_work / state.iterations()) + ")");
    }
}

// Define concepts for vehicle behaviors
template<typename T>
concept Acceleratable = requires(T v, double s) {
    { v.accelerate(s) } -> std::same_as<void>;
    { v.brake() } -> std::same_as<void>;
    { v.getWork() } -> std::same_as<double>;
};

template<typename T>
concept SportsCar = Acceleratable<T> && requires(T v) {
    { v.getBrand() } -> std::same_as<std::string>;
    { v.getModel() } -> std::same_as<std::string>;
};

// Concept-based implementations
class ElectricSportsCar {
    double speed = 0.0;
    double work = 0.0;
    std::vector<double> calculations;
    std::string brand;
    std::string model;
public:
    ElectricSportsCar(const std::string& b, const std::string& m) 
        : calculations(constants::VECTOR_SIZE), brand(b), model(m) {}
    void accelerate(const double s) {
        speed += s;
        impl::doAcceleration(speed, work, calculations);
    }
    void brake() { speed = 0; }
    double getWork() const { return work; }
    std::string getBrand() const { return brand; }
    std::string getModel() const { return model; }
};

class GasSportsCar {
    double speed = 0.0;
    double work = 0.0;
    std::vector<double> calculations;
    std::string brand;
    std::string model;
public:
    GasSportsCar(const std::string& b, const std::string& m) 
        : calculations(constants::VECTOR_SIZE), brand(b), model(m) {}
    void accelerate(const double s) {
        speed += s;
        impl::doAcceleration(speed, work, calculations);
    }
    void brake() { speed = 0; }
    double getWork() const { return work; }
    std::string getBrand() const { return brand; }
    std::string getModel() const { return model; }
};

// Base class for polymorphic comparison
class Vehicle {
public:
    virtual void accelerate(const double speed) = 0;
    virtual void brake() = 0;
    virtual void brake() = 0;
    virtual double getCurrentSpeed() = 0;    
    virtual double getFriction() = 0;        
    virtual ~Vehicle() = default;
};

// Add intermediate base class
class PolyVehicle : public Vehicle {
protected:
    double speed = 0.0;
    double work = 0.0;
    std::vector<double> calculations;

public:
    PolyVehicle() : calculations(constants::VECTOR_SIZE) {}
    virtual ~PolyVehicle() override = default;
    
    void accelerate(const double s) override { 
        speed += s * getFriction();
        impl::doAcceleration(speed, work, calculations);
    }
    
    double getCurrentSpeed() override { return speed; }
    double getWork() const { return work; }
    virtual void brake() override = 0;
};

// Simplify PolyElectricCar to use base class
class PolyElectricCar : public PolyVehicle {
public:
    double getFriction() override { return constants::ELECTRIC_FRICTION; }
    virtual ~PolyElectricCar() override = default;
    void brake() override { 
        // Regenerative braking: convert some kinetic energy back to work
        work += speed * constants::ELECTRIC_FRICTION;
        speed = 0;
    }
};

// Simplify PolyGasCar to use base class
class PolyGasCar : public PolyVehicle {
public:
    double getFriction() override { return constants::GAS_FRICTION; }
    virtual ~PolyGasCar() override = default;
    void brake() override {
        // Traditional brakes: just stop, no energy recovery
        speed = 0;
    }
};

// Remove VehicleType concept and accelerate_and_work function

static void BM_ConceptBased(benchmark::State& state) {
    static_assert(SportsCar<ElectricSportsCar>);
    static_assert(SportsCar<GasSportsCar>);
    
    ElectricSportsCar tesla("Tesla", "Model S");
    GasSportsCar porsche("Porsche", "911");
    
    // Use variant of pointers for type-safe access
    using CarPtr = std::variant<ElectricSportsCar*, GasSportsCar*>;
    CarPtr vehicles[] = { CarPtr(&tesla), CarPtr(&porsche) };
    double total_work = 0;
    
    for (auto _ : state) {
        for(int i = 0; i < constants::INNER_LOOP_COUNT; ++i) {
            auto& electric = *std::get<ElectricSportsCar*>(vehicles[0]);
            electric.accelerate(constants::ACCELERATION_VALUE);
            total_work += electric.getWork();
            electric.brake();
            
            auto& gas = *std::get<GasSportsCar*>(vehicles[1]);
            gas.accelerate(constants::ACCELERATION_VALUE);
            total_work += gas.getWork();
            gas.brake();
        }
        benchmark::ClobberMemory();
    }
    impl::reportStats(state, total_work);
}

static void BM_Polymorphic(benchmark::State& state) {
    PolyElectricCar tesla;
    PolyGasCar porsche;
    Vehicle* vehicles[] = {&tesla, &porsche};
    double total_work = 0;
    
    for (auto _ : state) {
        for(int i = 0; i < constants::INNER_LOOP_COUNT; ++i) {
            vehicles[0]->accelerate(constants::ACCELERATION_VALUE);
            total_work += tesla.getWork();
            vehicles[0]->brake();
            
            vehicles[1]->accelerate(constants::ACCELERATION_VALUE);
            total_work += porsche.getWork();
            vehicles[1]->brake();
        }
        benchmark::ClobberMemory();
    }
    impl::reportStats(state, total_work);
}

int main(int argc, char** argv) {
    // Register benchmarks with fewer iterations
    benchmark::RegisterBenchmark("ConceptBased", BM_ConceptBased)
        ->Unit(benchmark::kMillisecond)
        ->UseRealTime()
        ->MinTime(constants::MIN_TIME)
        ->Iterations(constants::BENCHMARK_ITERATIONS);
    
    benchmark::RegisterBenchmark("Polymorphic", BM_Polymorphic)
        ->Unit(benchmark::kMillisecond)
        ->UseRealTime()
        ->MinTime(constants::MIN_TIME)
        ->Iterations(constants::BENCHMARK_ITERATIONS);
    
    std::cout << "Starting benchmarks (should complete quickly)...\n";
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}

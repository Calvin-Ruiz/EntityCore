#ifndef SPHERE_HPP
#define SPHERE_HPP

// Generate a sphere with a precision level of n
// The total number of triangles is 20*pow(4, n)

struct SphereVertex {
    
}

class Sphere {
public:
    Sphere();
    virtual ~Sphere();

    // set precision level
    void setPrecision(n);
    // increase precision by one level
    void increasePrecision();
    // compile triangle
    void compile();
private:
    std::vector<
};

#endif /* end of include guard: SPHERE_HPP */

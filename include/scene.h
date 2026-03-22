#ifndef SCENE_H
#define SCENE_H

#include <math.h>
#include "vec3.h"
#include "ray.h"

#define EPSILON 1e-6

typedef struct {
    Vec3   ambient;
    Vec3   diffuse;
    Vec3   specular;
    double shininess;
} Material;

typedef struct {
    Vec3 position;
    Vec3 color;
} PointLight;

typedef struct {
    double   t;
    Vec3     point;
    Vec3     normal;
    Material mat;
} HitRecord;

typedef enum { GEOM_TRIANGLE=0, GEOM_SPHERE, GEOM_CONE } GeomType;

typedef struct {
    Vec3 v0,v1,v2,normal;
    Material mat;
} Triangle;

static inline void triangle_init(Triangle *t, Vec3 v0,Vec3 v1,Vec3 v2,Material mat){
    t->v0=v0; t->v1=v1; t->v2=v2;
    t->normal=vec3_normalize(vec3_cross(vec3_sub(v1,v0),vec3_sub(v2,v0)));
    t->mat=mat;
}

typedef struct { Vec3 center; double radius; Material mat; } Sphere;

/* Cone: apex頂點朝上，底面在 base_y（< apex.y），底面圓心在 (apex.x, base_y, apex.z) */
typedef struct {
    Vec3   apex;
    double base_y;
    double radius;
    Material side_mat;
    Material base_mat;
} Cone;

typedef struct {
    GeomType type;
    union { Triangle tri; Sphere sph; Cone cone; };
} Object;

static inline Object object_triangle(Vec3 v0,Vec3 v1,Vec3 v2,Material mat){
    Object o; o.type=GEOM_TRIANGLE; triangle_init(&o.tri,v0,v1,v2,mat); return o;
}
static inline Object object_sphere(Vec3 c,double r,Material mat){
    Object o; o.type=GEOM_SPHERE; o.sph.center=c; o.sph.radius=r; o.sph.mat=mat; return o;
}
static inline Object object_cone(Vec3 apex,double base_y,double radius,Material sm,Material bm){
    Object o; o.type=GEOM_CONE;
    o.cone.apex=apex; o.cone.base_y=base_y; o.cone.radius=radius;
    o.cone.side_mat=sm; o.cone.base_mat=bm;
    return o;
}

/* ── sphere intersect ── */
static inline int sphere_intersect(const Sphere *s, Ray ray, double t_min, double t_max, HitRecord *rec){
    Vec3 oc=vec3_sub(ray.origin,s->center);
    double b=vec3_dot(ray.dir,oc);
    double c=vec3_dot(oc,oc)-s->radius*s->radius;
    double disc=b*b-c;
    if(disc<0.0)return 0;
    double sq=sqrt(disc);
    double t=-b-sq;
    if(t<t_min||t>t_max){t=-b+sq; if(t<t_min||t>t_max)return 0;}
    Vec3 p=ray_at(ray,t);
    Vec3 n=vec3_normalize(vec3_sub(p,s->center));
    if(vec3_dot(ray.dir,n)>0.0)n=vec3_negate(n);
    rec->t=t; rec->point=p; rec->normal=n; rec->mat=s->mat;
    return 1;
}

/* ── cone intersect ──
 * 圓錐面方程（apex座標系，軸朝-Y）：
 *   x² + z² = k²·y²,   k = radius/H,  -H ≤ y ≤ 0
 */
static inline int cone_intersect(const Cone *cone, Ray ray, double t_min, double t_max, HitRecord *rec){
    double H=cone->apex.y-cone->base_y;
    if(H<EPSILON)return 0;
    double k=cone->radius/H, k2=k*k;

    Vec3 oc=vec3_sub(ray.origin,cone->apex);
    double dx=ray.dir.x,dy=ray.dir.y,dz=ray.dir.z;
    double ox=oc.x,oy=oc.y,oz=oc.z;

    double a=dx*dx+dz*dz-k2*dy*dy;
    double b=ox*dx+oz*dz-k2*oy*dy;
    double c=ox*ox+oz*oz-k2*oy*oy;

    int hit=0; double best_t=t_max; HitRecord br;

    /* 側面 */
    if(fabs(a)>EPSILON){
        double disc=b*b-a*c;
        if(disc>=0.0){
            double sq=sqrt(disc);
            double roots[2]={(-b-sq)/a,(-b+sq)/a};
            for(int i=0;i<2;++i){
                double t=roots[i];
                if(t<t_min||t>=best_t)continue;
                Vec3 p=ray_at(ray,t);
                double py=p.y-cone->apex.y;
                if(py>EPSILON||py<-H-EPSILON)continue;
                double px=p.x-cone->apex.x, pz=p.z-cone->apex.z;
                double lat=sqrt(px*px+pz*pz);
                Vec3 n=vec3_normalize(vec3(px, lat*k, pz));
                if(vec3_dot(ray.dir,n)>0.0)n=vec3_negate(n);
                best_t=t; br=(HitRecord){t,p,n,cone->side_mat}; hit=1;
            }
        }
    }

    /* 底面圓盤 */
    if(fabs(ray.dir.y)>EPSILON){
        double t=(cone->base_y-ray.origin.y)/ray.dir.y;
        if(t>=t_min&&t<best_t){
            Vec3 p=ray_at(ray,t);
            double rx=p.x-cone->apex.x, rz=p.z-cone->apex.z;
            if(rx*rx+rz*rz<=cone->radius*cone->radius){
                Vec3 n=vec3(0.0,-1.0,0.0);
                if(vec3_dot(ray.dir,n)>0.0)n=vec3_negate(n);
                best_t=t; br=(HitRecord){t,p,n,cone->base_mat}; hit=1;
            }
        }
    }

    if(hit)*rec=br;
    return hit;
}

/* ── triangle intersect (Möller–Trumbore) ── */
static inline int triangle_intersect(const Triangle *tri, Ray ray, double t_min, double t_max, HitRecord *rec){
    Vec3 e1=vec3_sub(tri->v1,tri->v0),e2=vec3_sub(tri->v2,tri->v0);
    Vec3 h=vec3_cross(ray.dir,e2);
    double a=vec3_dot(e1,h);
    if(fabs(a)<EPSILON)return 0;
    double f=1.0/a;
    Vec3 s=vec3_sub(ray.origin,tri->v0);
    double u=f*vec3_dot(s,h);
    if(u<0.0||u>1.0)return 0;
    Vec3 q=vec3_cross(s,e1);
    double v=f*vec3_dot(ray.dir,q);
    if(v<0.0||u+v>1.0)return 0;
    double t=f*vec3_dot(e2,q);
    if(t<t_min||t>t_max)return 0;
    Vec3 n=tri->normal;
    if(vec3_dot(ray.dir,n)>0.0)n=vec3_negate(n);
    rec->t=t; rec->point=ray_at(ray,t); rec->normal=n; rec->mat=tri->mat;
    return 1;
}

/* ── 統一求交 ── */
static inline int object_intersect(const Object *obj, Ray ray, double t_min, double t_max, HitRecord *rec){
    switch(obj->type){
        case GEOM_TRIANGLE: return triangle_intersect(&obj->tri, ray,t_min,t_max,rec);
        case GEOM_SPHERE:   return sphere_intersect  (&obj->sph, ray,t_min,t_max,rec);
        case GEOM_CONE:     return cone_intersect    (&obj->cone,ray,t_min,t_max,rec);
        default: return 0;
    }
}

#endif /* SCENE_H */

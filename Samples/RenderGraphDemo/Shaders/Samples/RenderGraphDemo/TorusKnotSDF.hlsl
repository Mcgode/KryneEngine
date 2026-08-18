// SDF functions
// Parametric equation of the (p, q)-Torus Knot.
float3 TorusKnot(in float _t, in float _R, in float _pParam, in float _qParam) {
    return float3(
        _R * (2 + cos(_qParam * _t)) * cos(_pParam * _t) * 0.5,
        _R * (2 + cos(_qParam * _t)) * sin(_pParam * _t) * 0.5,
        _R * sin(_qParam * _t) * 0.5
    );
}

// First derivative of the parametric equation.
float3 TorusKnotDerived(in float _t, in float _R, in float _pParam, in float _qParam) {
    const float pt = _pParam * _t;
    const float qt = _qParam * _t;

    return float3(
        -0.5 * _pParam * _R * sin(pt) * (cos(qt) + 2) - 0.5 * _qParam * _R * cos(pt) * sin(qt),
         0.5 * _pParam * _R * cos(pt) * (cos(qt) + 2) - 0.5 * _qParam * _R * sin(pt) * sin(qt),
         0.5 * _qParam * _R * cos(qt)
    );
}

// Distance between (0, 0, 0) and a line through p with direction d.
float DistanceOriginLine(in float3 p, in float3 d) {
    return length(cross(p, p + normalize(d)));
}

// Distance between p and torusKnot(t).
// The better the estimate of t is, the more accurate this distance will be.
float DistancePointCurve(in float3 _p, in float _R, in int _pParam, in int _qParam, in float _s, in float _t) {
    return DistanceOriginLine(_p - _s * TorusKnot(_t, _R, _pParam, _qParam), TorusKnotDerived(_t, _R, _pParam, _qParam));
}

// Rough estimate of t, trying to get torusKnot(t) near p.
float RoughInverseTorusKnot(in float3 _p, in int _pParam, in int _qParam) {
    return atan2(_p.y, _p.x) / float(_pParam);
}

// SDF for Torus Knot.
// R = major radius (size of the torus)
// r = minor radius (thickness of the wire)
// pParam, qParam = parameters defining the (p, q)-torus knot
float SdTorusKnot(in float3 _p, in float _R, in float _r, in int _pParam, in int _qParam) {
    float t = RoughInverseTorusKnot(_p, _pParam, _qParam);

    // There are multiple points on the curve that match t; calculate both.
    float d1 = DistancePointCurve(_p, _R, _pParam, _qParam, 1.0, t);
    float d2 = DistancePointCurve(_p, _R, _pParam, _qParam, 1.0, t + 3.141592653589793); // t + PI

    // Take whichever is closest to p.
    return min(d1, d2) - _r;
}
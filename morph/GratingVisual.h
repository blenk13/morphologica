/*
 * A VisualModel to show a grating of straight bars at any angle and in any two colours. A time can
 * be set so that the grating can be moved in time according to a 'front velocity'
 *
 * Author: Seb James
 * Date: July 2024
 */

#pragma once

#include <array>
#include <bitset>

#include <morph/VisualModel.h>
#include <morph/vec.h>
#include <morph/mathconst.h>
#include <morph/colour.h>
#include <morph/MathAlgo.h>

namespace morph {

    enum class border_id {
        top,
        bottom,
        left,
        right,
        unknown
    };

    std::string border_id_str (const border_id& id)
    {
        std::string rtn("unknown");
        if (id == border_id::top) {
            rtn = "top";
        } else if (id == border_id::bottom) {
            rtn = "bottom";
        } else if (id == border_id::left) {
            rtn = "left";
        } else if (id == border_id::right) {
            rtn = "right";
        }
        return rtn;
    }

    //! This class creates the vertices for a rectangular moving grating
    template<int glver = morph::gl::version_4_1>
    class GratingVisual : public VisualModel<glver>
    {
    public:
        GratingVisual() { this->mv_offset = {0.0, 0.0, 0.0}; }
        GratingVisual (const vec<float, 3> _offset) { this->init (_offset); }
        ~GratingVisual () {}

        void init (const vec<float, 3> _offset)
        {
            this->mv_offset = _offset;
            this->viewmatrix.translate (this->mv_offset);
        }

        void draw_band (const vec<float, 2>& fp1, const vec<float, 2>& fq1,
                        const vec<float, 2>& fp2, const vec<float, 2>& fq2,
                        const std::array<float, 3>& _col)
        {
            this->vertex_push (fp1.plus_one_dim(), this->vertexPositions);
            this->vertex_push (fq1.plus_one_dim(), this->vertexPositions);
            this->vertex_push (fp2.plus_one_dim(), this->vertexPositions);
            this->vertex_push (fq2.plus_one_dim(), this->vertexPositions);
            for (unsigned int vi = 0; vi < 4; ++vi) {
                this->vertex_push (_col, this->vertexColors);
                this->vertex_push (this->uz, this->vertexNormals);
            }
            this->indices.push_back (this->idx);
            this->indices.push_back (this->idx+1);
            this->indices.push_back (this->idx+2);
            this->indices.push_back (this->idx+2);
            this->indices.push_back (this->idx+1);
            this->indices.push_back (this->idx+3);
            this->idx += 4;
        }

        // Swap _p1 and _p2 along with their border ids
        void swap_pair (vec<float, 2>& _p1, vec<float, 2>& _p2, border_id& _p1_id, border_id& _p2_id)
        {
            auto tmp = _p2;
            auto tmp_id = _p2_id;
            _p2 = _p1;
            _p2_id = _p1_id;
            _p1 = tmp;
            _p1_id = tmp_id;
        }

        void initializeVertices()
        {
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();
            this->indices.clear();

            // The velocity offset for each location of each front
            vec<float, 2> v_offset = this->v_front * this->t;

            // unit vector in x dirn
            constexpr vec<float, 2> u_x = { 1.0f, 0.0f };

            // The unit vector perpendicular to the front angle
            vec<float, 2> u_alpha = u_x;
            u_alpha.set_angle (morph::mathconst<float>::deg2rad * this->alpha);
            vec<float, 2> u_alpha_perp = u_x;
            u_alpha_perp.set_angle (morph::mathconst<float>::pi_over_2 + morph::mathconst<float>::deg2rad * this->alpha);

            // Corners
            vec<float, 2> top_left =  vec<float, 2>{ this->mv_offset[0],             this->mv_offset[1] + dims[1] };
            vec<float, 2> bot_left =  vec<float, 2>{ this->mv_offset[0],             this->mv_offset[1]           }; // or mv_offset
            vec<float, 2> top_right = vec<float, 2>{ this->mv_offset[0] + dims[0],   this->mv_offset[1] + dims[1] }; // or mv_offset + dims
            vec<float, 2> bot_right = vec<float, 2>{ this->mv_offset[0] + dims[0],   this->mv_offset[1]           };

            // Line segments of the borders. Will need these for computing line crossings. each line segment is 'pq'
            vec<float, 2> bot_p = bot_left;
            vec<float, 2> bot_q = bot_right;
            //
            vec<float, 2> top_p = top_left;
            vec<float, 2> top_q = top_right;
            //
            vec<float, 2> left_p = bot_left;
            vec<float, 2> left_q = top_left;
            //
            vec<float, 2> right_p = bot_right;
            vec<float, 2> right_q = top_right;

            /**
             * Subroutine for finding the band vertices on the boundary given as a lambda
             *
             * This finds the two points at which a line segment passing over the rectangle
             * intersects. If the line segment passes through a corner of the rectangle, it may
             * intersect with three points. In this case, avoid placing two identical points in fp
             * and fq, instead, place points that are far apart.
             */
            auto find_border_points =
            [bot_p, bot_q, top_p, top_q, left_p, left_q, right_p, right_q]
            (const vec<float, 2>& _p, const vec<float, 2>& _q,
             vec<float, 2>& fp, vec<float, 2>& fq,
             border_id& fp_id, border_id& fq_id,
             const std::bitset<2>& _bi,
             const std::bitset<2>& _ti,
             const std::bitset<2>& _li,
             const std::bitset<2>& _ri)
            {
                constexpr bool debug_this = false;

                // How close do intersection points need to be to be considered to be the same?
                constexpr float thresh = 10.0f * std::numeric_limits<float>::epsilon();

                if constexpr (debug_this) {
                    std::cout << "in find_border_points with line "
                              << _p << "_" << _q << " B4, fp_id:" << border_id_str(fp_id)
                              << " fq_id:" << border_id_str(fq_id) << std::endl;

                    std::cout << "  _bi: " << _bi
                              << "  _ti: " << _ti
                              << "  _li: " << _li
                              << "  _ri: " << _ri
                              << std::endl;
                }

                if (_bi.test(0)) { // bottom
                    if constexpr (debug_this) { std::cout << "(B...)\n"; }
                    fp = morph::MathAlgo::crossing_point (_p, _q, bot_p, bot_q);
                    fp_id = border_id::bottom;
                    if (_ti.test(0)) {
                        // bottom and top edges
                        if constexpr (debug_this) { std::cout << "(BT)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, top_p, top_q);
                        fq_id = border_id::top;

                    } else if (_li.test(0)) {
                        // bottom and left edges
                        if constexpr (debug_this) { std::cout << "(BL)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, left_p, left_q);
                        fq_id = border_id::left;

                        // What if it ALSO intersects with the right edge? And the bottom/left
                        // intersects are the same place?
                        if (_ti.test(0)) {
                            // There's a third intersection. Was B==L?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace L with T:
                                fq = morph::MathAlgo::crossing_point (_p, _q, top_p, top_q);
                                fq_id = border_id::top;
                            }
                        } else if (_ri.test(0)) {
                            // There's a third intersection. Was B==L?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace L with R:
                                fq = morph::MathAlgo::crossing_point (_p, _q, right_p, right_q);
                                fq_id = border_id::right;
                            }
                        }


                    } else if (_ri.test(0)) {
                        // bottom and right edges
                        if constexpr (debug_this) { std::cout << "(BR)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, right_p, right_q);
                        fq_id = border_id::right;

                        if (_ti.test(0)) {
                            // There's a third intersection. Was B==R?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace R with T:
                                fq = morph::MathAlgo::crossing_point (_p, _q, top_p, top_q);
                                fq_id = border_id::top;
                            }
                        }

                    } else {
                        throw std::runtime_error ("unexpected1");
                    }

                } else if (_ti.test(0)) {

                    if constexpr (debug_this) { std::cout << "(T...)\n"; }
                    fp = morph::MathAlgo::crossing_point (_p, _q, top_p, top_q);
                    fp_id = border_id::top;

                    if (_li.test(0)) {
                        // top and left
                        if constexpr (debug_this) { std::cout << "(TL)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, left_p, left_q);
                        fq_id = border_id::left;

                        // Third intersection tests
                        if (_bi.test(0)) {
                            // There's a third intersection. Was T==L?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace L with B:
                                fq = morph::MathAlgo::crossing_point (_p, _q, bot_p, bot_q);
                                fq_id = border_id::bottom;
                            }
                        } else if (_ri.test(0)) {
                            // There's a third intersection. Was T==L?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace L with R:
                                fq = morph::MathAlgo::crossing_point (_p, _q, right_p, right_q);
                                fq_id = border_id::right;
                            }
                        }

                    } else if (_ri.test(0)) {
                        // top and right
                        if constexpr (debug_this) { std::cout << "(TR)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, right_p, right_q);
                        fq_id = border_id::right;

                        if (_bi.test(0)) {
                            // There's a third intersection. Was T==R?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace R with B:
                                fq = morph::MathAlgo::crossing_point (_p, _q, bot_p, bot_q);
                                fq_id = border_id::bottom;
                            }
                        } else if (_li.test(0)) {
                            // There's a third intersection. Was T==R?
                            if ((fp-fq).length() < thresh) {
                                // If so, replace R with L:
                                fq = morph::MathAlgo::crossing_point (_p, _q, left_p, left_q);
                                fq_id = border_id::left;
                            }
                        }

                    } else {
                        throw std::runtime_error ("unexpected2");
                    }
                } else if (_li.test(0)) {
                    if constexpr (debug_this) { std::cout << "(L...)\n"; }
                    fp = morph::MathAlgo::crossing_point (_p, _q, left_p, left_q);
                    fp_id = border_id::left;

                    if (_ri.test(0)) {
                        // left and right
                        if constexpr (debug_this) { std::cout << "(LR)\n"; }
                        fq = morph::MathAlgo::crossing_point (_p, _q, right_p, right_q);
                        fq_id = border_id::right;
                    } else {
                        throw std::runtime_error ("unexpected3");
                    }
                } else if (_ri.test(0)) {
                    throw std::runtime_error ("We have ri first (unexpected?)");

                } // else there are no intersections

                if constexpr (debug_this) {
                    std::cout << "in find_border_points. AT END, fp:" << fp << "/" << border_id_str(fp_id)
                              << " fq:" << fq << "/" << border_id_str(fq_id) << std::endl;
                }

            }; // end of find_border_points()

            /**
             * Lambda to draw triangle/quadrilateral fill in shape given two points and their
             * border intersection identifications.
             */
            auto draw_fill_in_shape = [this, top_left, bot_left, bot_right, top_right]
            (const vec<float, 2>& _p, const vec<float, 2>& fp, const vec<float, 2>& fq,
             const border_id& fp_id, const border_id& fq_id, const std::array<float, 3>& _col)
            {
                vec<float, 2> corner = { 0, 0 };
                vec<float, 2> corner_2 = { -100, -100 };
                if ((fp_id == border_id::left && fq_id == border_id::top)
                    || (fq_id == border_id::left && fp_id == border_id::top)) {
                    corner = top_left;
                } else if ((fp_id == border_id::left && fq_id == border_id::bottom)
                           || (fq_id == border_id::left && fp_id == border_id::bottom)) {
                    corner = bot_left;
                } else if ((fp_id == border_id::right && fq_id == border_id::bottom)
                           || (fq_id == border_id::right && fp_id == border_id::bottom)) {
                    corner = bot_right;
                } else if ((fp_id == border_id::right && fq_id == border_id::top)
                           || (fq_id == border_id::right && fp_id == border_id::top)) {
                    corner = top_right;

                } else if ((fp_id == border_id::bottom && fq_id == border_id::top)
                           || (fq_id == border_id::bottom && fp_id == border_id::top)) {
                    // vertical bands
                    float d_to_left = (_p - bot_left).length();
                    float d_to_right = (_p - bot_right).length();
                    corner = d_to_left < d_to_right ? bot_left : bot_right;
                    corner_2 = d_to_left < d_to_right ? top_left : top_right;

                } else if ((fp_id == border_id::left && fq_id == border_id::right)
                           || (fq_id == border_id::left && fp_id == border_id::right)) {
                    // horz bands. Top or bottom? As long as bands are smaller than the height
                    // of the rectangle, we can use the closest.
                    float d_to_top = (_p - top_left).length();
                    float d_to_bottom = (_p - bot_left).length();
                    corner = d_to_top < d_to_bottom ? top_left : bot_left;
                    corner_2 = d_to_top < d_to_bottom ? top_right : bot_right;

                } else {
                    throw std::runtime_error ("unexpected corner");
                }

                if (corner_2 == morph::vec<float, 2>{-100, -100 }) {
                    // Draw triangle
                    this->vertex_push (fp.plus_one_dim(), this->vertexPositions);
                    this->vertex_push (fq.plus_one_dim(), this->vertexPositions);
                    this->vertex_push (corner.plus_one_dim(), this->vertexPositions);
                    for (unsigned int vi = 0; vi < 3; ++vi) {
                        this->vertex_push (_col, this->vertexColors);
                        this->vertex_push (this->uz, this->vertexNormals);
                        this->indices.push_back (this->idx++);
                    }
                } else {
                    // Draw quatrilateral
                    this->vertex_push (fp.plus_one_dim(), this->vertexPositions);
                    this->vertex_push (corner.plus_one_dim(), this->vertexPositions);
                    this->vertex_push (fq.plus_one_dim(), this->vertexPositions);
                    this->vertex_push (corner_2.plus_one_dim(), this->vertexPositions);
                    for (unsigned int vi = 0; vi < 4; ++vi) {
                        this->vertex_push (_col, this->vertexColors);
                        this->vertex_push (this->uz, this->vertexNormals);
                    }
                    this->indices.push_back (this->idx);
                    this->indices.push_back (this->idx+1);
                    this->indices.push_back (this->idx+2);
                    this->indices.push_back (this->idx+2);
                    this->indices.push_back (this->idx+1);
                    this->indices.push_back (this->idx+3);
                    this->idx += 4;
                }
            }; // end of draw_fill_in_shape()

            // How does one band wavelength project onto the x and y axes?
            float length_of_lambda_in_x = this->lambda / std::cos (morph::mathconst<float>::deg2rad * this->alpha);
            float length_of_lambda_in_y = this->lambda / std::sin (morph::mathconst<float>::deg2rad * this->alpha);

            // p_0 is our starting location to draw bands
            vec<float, 2> p_0 = { 0.0f, 0.0f };
            if (std::abs(length_of_lambda_in_x) > std::abs(dims[0])) {
                // In this case we have horizontal bands, so start from a p_0 on the y axis
                float y_lambdas_f = v_offset[1] / length_of_lambda_in_y;
                float lambdas_y = std::trunc (y_lambdas_f);
                float lambdas_y_offset = lambdas_y * length_of_lambda_in_y;
                p_0[1] = v_offset[1] - lambdas_y_offset;
            } else {
                // Bands are roughly vertical, place p_0 on the x axis
                float x_lambdas_f = v_offset[0] / length_of_lambda_in_x;
                float lambdas_x = std::trunc (x_lambdas_f);
                float lambdas_x_offset = lambdas_x * length_of_lambda_in_x;
                p_0[0] = v_offset[0] - lambdas_x_offset;
            }

            // This vector is the distance to travel from a point within the rectangle to make half
            // of the wavefront that will be guaranteed to intersect with the rectangle border.
            vec<float, 2> half_wave = dims.length() * u_alpha_perp;

            /**
             * Execute a loop twice. I used a lambda to make this a subroutine without needing to
             * make all the variables into members of the GratingVisual class.
             */
            auto loop_lambda = [this, find_border_points, draw_fill_in_shape, p_0, half_wave,
                                bot_p, bot_q, top_p, top_q, left_p, left_q, right_p, right_q,
                                bot_left, top_left, bot_right, top_right]
            (unsigned int i, const vec<float, 2>& p_step)
            {
                vec<float, 2> p1 = {0,0}, q1 = {0,0}, p2 = {0,0}, q2 = {0,0};
                vec<float, 2> fp1 = {0,0}, fp2 = {0,0}, fq1 = {0,0}, fq2 = {0,0};

                // Identifiers for the final crossing points.
                border_id fp1_id = border_id::unknown;
                border_id fq1_id = border_id::unknown;
                border_id fp2_id = border_id::unknown;
                border_id fq2_id = border_id::unknown;

                for (vec<float, 2> p = p_0; ; p += p_step) {

                    std::array<float, 3> col = i%2==0 ? colour1 : colour2;
                    if constexpr (debug_geometry) {
                        col = i%2==0 ? morph::colour::seagreen3 : morph::colour::turquoiseblue;
                    }
                    i++;

                    // First line of a 'band' p1-q1
                    p1 = p + half_wave;
                    q1 = p - half_wave;

                    // Compute intersections for p1, q1
                    std::bitset<2> bi = morph::MathAlgo::segments_intersect (p1, q1, bot_p, bot_q);
                    std::bitset<2> ti = morph::MathAlgo::segments_intersect (p1, q1, top_p, top_q);
                    std::bitset<2> li = morph::MathAlgo::segments_intersect (p1, q1, left_p, left_q);
                    std::bitset<2> ri = morph::MathAlgo::segments_intersect (p1, q1, right_p, right_q);

                    // Check colinearity; in which case set fp1 & fq1 to the relevant corners.
                    bool first_colin = true;
                    if (bi.test(1)) { // colinear with bottom rectangle border
                        fp1 = bot_left;   fp1_id = border_id::bottom;
                        fq1 = bot_right;  fq1_id = border_id::bottom;
                    } else if (ti.test(1)) {
                        fp1 = top_left;   fp1_id = border_id::top;
                        fq1 = top_right;  fq1_id = border_id::top;
                    } else if (li.test(1)) {
                        fp1 = bot_left;   fp1_id = border_id::left;
                        fq1 = top_left;   fq1_id = border_id::left;
                    } else if (ri.test(1)) {
                        fp1 = bot_right;  fp1_id = border_id::right;
                        fq1 = top_right;  fq1_id = border_id::right;
                    } else {
                        first_colin = false;
                        // Test if we're off the rectangle
                        if (!bi.test(0) && !ti.test(0) && !li.test(0) && !ri.test(0)) {
                            if constexpr (this->debug_text) {
                                std::cout << "Off rectangle for p1=" << p1 << ", q1=" << q1 << "; will break\n";
                            }
                            break;
                        }
                        // From p1, q1 find fp1 and fp1_id
                        find_border_points (p1, q1,  fp1, fq1, fp1_id, fq1_id, bi, ti, li, ri);
                        //std::cout << "Determine fp1 fq1 from find_border_points: " << fp1 << "," << fq1 << std::endl;
                    }

                    // Second line (p2-q2)
                    p2 = p + p_step + half_wave;
                    q2 = p + p_step - half_wave;

                    // repeat computation of intersections for p2, q2.
                    bi = morph::MathAlgo::segments_intersect (p2, q2, bot_p, bot_q);
                    ti = morph::MathAlgo::segments_intersect (p2, q2, top_p, top_q);
                    li = morph::MathAlgo::segments_intersect (p2, q2, left_p, left_q);
                    ri = morph::MathAlgo::segments_intersect (p2, q2, right_p, right_q);

                    if (bi.test(1)) { // colinear with bottom rectangle border
                        fp2 = bot_left;   fp2_id = border_id::bottom;
                        fq2 = bot_right;  fq2_id = border_id::bottom;
                    } else if (ti.test(1)) {
                        fp2 = top_left;   fp2_id = border_id::top;
                        fq2 = top_right;  fq2_id = border_id::top;
                    } else if (li.test(1)) {
                        fp2 = bot_left;   fp2_id = border_id::left;
                        fq2 = top_left;   fq2_id = border_id::left;
                    } else if (ri.test(1)) {
                        fp2 = bot_right;  fp2_id = border_id::right;
                        fq2 = top_right;  fq2_id = border_id::right;
                    } else {
                        // Test if the *second* line of a band is off the rectangle
                        if (!bi.test(0) && !ti.test(0) && !li.test(0) && !ri.test(0)) {
                            if (!first_colin) {
                                // Draw fill in shape using the first line
                                if constexpr (debug_geometry) {
                                    draw_fill_in_shape (p, fp1, fq1, fp1_id, fq1_id, morph::colour::crimson);
                                } else {
                                    draw_fill_in_shape (p, fp1, fq1, fp1_id, fq1_id, col);
                                }
                            }
                            break;
                        }
                        find_border_points (p2, q2,  fp2, fq2, fp2_id, fq2_id, bi, ti, li, ri);
                        if constexpr (debug_text) {
                            std::cout << "find_border_points result: fp2/fq2: " << fp2 << "/" << fq2
                                      <<" fp2_id/fq2_id: "
                                      << border_id_str (fp2_id) << "/" << border_id_str (fq2_id) << std::endl;
                        }
                    }

                    // We now have all 4 band vertex points. Do a sanity check
                    if (fp1_id == border_id::unknown || fq1_id == border_id::unknown
                        || fp2_id == border_id::unknown || fq2_id == border_id::unknown) {
                        if constexpr (debug_text) {
                            std::cout << "border ids:"
                                      << " fp1:" << border_id_str (fp1_id)
                                      << " fq1:" << border_id_str (fq1_id)
                                      << " fp2:" << border_id_str (fp2_id)
                                      << " fq2:" << border_id_str (fq2_id) << std::endl;
                        }
                        throw std::runtime_error ("unexpected border_id");
                    }

                    // Does fp1-fp2 intersect with fq1-fq2? (if so triangles will draw badly so swap a pair)
                    std::bitset<2> fpi = morph::MathAlgo::segments_intersect (fp1, fp2, fq1, fq2);
                    if (fpi.test(0)) { this->swap_pair (fp2, fq2, fp2_id, fq2_id); }

                    // Determine if fill-in triangles should be drawn
                    if (fp1_id != fp2_id) {
                        if constexpr (debug_geometry) {
                            draw_fill_in_shape (p, fp1, fp2, fp1_id, fp2_id, morph::colour::royalblue);
                        } else {
                            draw_fill_in_shape (p, fp1, fp2, fp1_id, fp2_id, col);
                        }
                    }

                    if (fq1_id != fq2_id) {
                        if constexpr (debug_geometry) {
                            draw_fill_in_shape (p, fq1, fq2, fq1_id, fq2_id, morph::colour::yellow);
                        } else {
                            draw_fill_in_shape (p, fq1, fq2, fq1_id, fq2_id, col);
                        }
                    }

                    this->draw_band (fp1, fq1, fp2, fq2, col);

                    if constexpr (debug_geometry) {
                        this->computeSphere (this->idx, p1.plus_one_dim(), colour1, 0.02f, 16, 20);
                        this->computeSphere (this->idx, q1.plus_one_dim(), colour1, 0.02f, 16, 20);
                        this->computeSphere (this->idx, p2.plus_one_dim(), colour2, 0.02f, 16, 20);
                        this->computeSphere (this->idx, q2.plus_one_dim(), colour2, 0.02f, 16, 20);
                        this->computeSphere (this->idx, fp1.plus_one_dim(), morph::colour::crimson, 0.01f, 16, 20);
                        this->computeSphere (this->idx, fq1.plus_one_dim(), morph::colour::violetred2, 0.01f, 16, 20);
                        this->computeSphere (this->idx, fp2.plus_one_dim(), morph::colour::royalblue, 0.01f, 16, 20);
                        this->computeSphere (this->idx, fq2.plus_one_dim(), morph::colour::dodgerblue1, 0.01f, 16, 20);
                    }
                }
            }; // end of loop lambda

            vec<float, 2> p_step = 0.5f * lambda * u_alpha;
            // Run the band drawing loop forwards...
            loop_lambda (0, p_step);
            // ...and backwards
            loop_lambda (1, -p_step);


            if constexpr (debug_geometry) {
                // Seeing boundary useful for debugging
                constexpr float bwid = 0.005f;
                constexpr morph::vec<float, 2> voffs = {0.0f, bwid/2.0f};
                constexpr morph::vec<float, 2> hoffs = {bwid/2.0f, 0.0f};
                constexpr morph::vec<float, 2> hoffs2 = {bwid, 0.0f};
                this->computeFlatLine (this->idx, (bot_p-voffs-hoffs2).plus_one_dim(), (bot_q-voffs+hoffs2).plus_one_dim(),
                                       this->uz, morph::colour::black, bwid);
                this->computeFlatLine (this->idx, (right_p+hoffs).plus_one_dim(), (right_q+hoffs).plus_one_dim(),
                                       this->uz, morph::colour::black, bwid);
                this->computeFlatLine (this->idx, (top_p+voffs-hoffs2).plus_one_dim(), (top_q+voffs+hoffs2).plus_one_dim(),
                                       this->uz, morph::colour::black, bwid);
                this->computeFlatLine (this->idx, (left_p-hoffs).plus_one_dim(), (left_q-hoffs).plus_one_dim(),
                                       this->uz, morph::colour::black, bwid);

                // Also show the v_front vector
                morph::vec<float> vfstart = { -2.0f * this->v_front.length(), 0, 0};
                this->computeArrow (vfstart, vfstart + this->v_front.plus_one_dim(), morph::colour::black);
            }
        }

        //! The colours of the bands
        std::array<float, 3> colour1 = morph::colour::white;
        std::array<float, 3> colour2 = morph::colour::black;
        //! The velocity of the fronts.
        vec<float, 2> v_front = { 0.0f, 0.0f };
        //! The wavelength of the fronts
        float lambda = 0.1f;
        //! The angle of the fronts, wrt x.
        float alpha = 45.0f;
        //! Width, height (after any rotation?)
        vec<float, 2> dims = { 2.0f, 1.0f };
        //! Current time
        unsigned long long int t = 0;
        //! Draw in colours that are helpful for debugging?
        static constexpr bool debug_geometry = false;
        static constexpr bool debug_text = false;
    };

} // namespace morph
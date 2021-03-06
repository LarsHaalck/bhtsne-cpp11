/*
 *
 * Copyright (c) 2014, Laurens van der Maaten (Delft University of Technology)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the Delft University of Technology.
 * 4. Neither the name of the Delft University of Technology nor the names of
 *    its contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAURENS VAN DER MAATEN ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL LAURENS VAN DER MAATEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include "sptree.h"
#include "cell.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace tsne
{
SPTree::SPTree(int D, const std::vector<double>& inp_data)
    : dimension(D)
    , is_leaf(true)
    , size(0)
    , cum_size(0)
    , boundary(nullptr)
    , data(inp_data)
    , center_of_mass(D)
    , children()
    , no_children(2)
{
    for (int d = 1; d < D; d++)
        no_children *= 2;
    children = std::vector<std::unique_ptr<SPTree>>(no_children);
}
// Default constructor for SPTree -- build tree, too!
SPTree::SPTree(int D, const std::vector<double>& inp_data, int N)
    : SPTree(D, inp_data)
{
    // Compute mean, width, and height of current map (boundaries of SPTree)
    int nD = 0;
    auto mean_Y = std::vector<double>(D);
    auto min_Y = std::vector<double>(D, std::numeric_limits<double>::max());
    auto max_Y = std::vector<double>(D, -std::numeric_limits<double>::max());
    for (int n = 0; n < N; n++)
    {
        for (int d = 0; d < D; d++)
        {
            mean_Y[d] += inp_data[n * D + d];
            if (inp_data[nD + d] < min_Y[d])
                min_Y[d] = inp_data[nD + d];
            if (inp_data[nD + d] > max_Y[d])
                max_Y[d] = inp_data[nD + d];
        }
        nD += D;
    }
    for (int d = 0; d < D; d++)
        mean_Y[d] /= static_cast<double>(N);

    auto width = std::vector<double>();
    width.reserve(D);
    for (int d = 0; d < D; d++)
        width.push_back(std::max(max_Y[d] - mean_Y[d], mean_Y[d] - min_Y[d]) + 1e-5);

    boundary = std::make_unique<Cell>(dimension, std::move(mean_Y), std::move(width));
    fill(N);
}

// Constructor for SPTree with particular size -- build the tree, too!
/*SPTree::SPTree(int D, const std::vector<double>& inp_data, int N,
    std::vector<double>&& inp_corner,
    std::vector<double>&& inp_width)
    : SPTree(D, inp_data)
{
    boundary = std::make_unique<Cell>(dimension, std::move(inp_corner),
        std::move(inp_width));
    fill(N);
}*/

// Constructor for SPTree with particular size (do not fill the tree)
SPTree::SPTree(int D, const std::vector<double>& inp_data,
    std::vector<double>&& inp_corner, std::vector<double>&& inp_width)
    : SPTree(D, inp_data)
{
    boundary
        = std::make_unique<Cell>(dimension, std::move(inp_corner), std::move(inp_width));
}

// Insert a point into the SPTree
bool SPTree::insert(int new_index)
{
    // Ignore objects which do not belong in this quad tree
    // point array starts at data + new_index * dimension
    int offset = new_index * dimension;
    if (!boundary->containsPoint(data, offset))
        return false;

    // Online update of cumulative size and center-of-mass
    cum_size++;
    double mult1 = static_cast<double>(cum_size - 1) / static_cast<double>(cum_size);
    double mult2 = 1.0 / static_cast<double>(cum_size);
    for (int d = 0; d < dimension; d++)
        center_of_mass[d] = center_of_mass[d] * mult1 + mult2 * data[d + offset];

    // If there is space in this quad tree and it is a leaf, add the object here
    if (is_leaf && size < QT_NODE_CAPACITY)
    {
        index[size] = new_index;
        size++;
        return true;
    }

    // Don't add duplicates for now (this is not very nice)
    bool any_duplicate = false;
    for (int n = 0; n < size; n++)
    {
        bool duplicate = true;
        for (int d = 0; d < dimension; d++)
        {
            if (data[d + offset] != data[index[n] * dimension + d])
            {
                duplicate = false;
                break;
            }
        }
        any_duplicate = any_duplicate | duplicate;
    }
    if (any_duplicate)
        return true;

    // Otherwise, we need to subdivide the current cell
    if (is_leaf)
        subdivide();

    // Find out where the point can be inserted
    for (int i = 0; i < no_children; i++)
    {
        if (children[i]->insert(new_index))
            return true;
    }

    // Otherwise, the point cannot be inserted (this should never happen)
    return false;
}

// Create four children which fully divide this cell into four quads of equal area
void SPTree::subdivide()
{
    // Create new children
    /*auto new_width = std::vector<double>();
    new_width.reserve(dimension);
    for (int d = 0; d < dimension; d++)
        new_width.push_back(0.5 * boundary->getWidth(d));*/

    for (int i = 0; i < no_children; i++)
    {
        auto new_corner = std::vector<double>();
        auto new_width = std::vector<double>();
        new_corner.reserve(dimension);
        new_width.reserve(dimension);
        int div = 1;
        for (int d = 0; d < dimension; d++)
        {
            new_width.push_back(0.5 * boundary->getWidth(d));
            if ((i / div) % 2 == 1)
                new_corner.push_back(
                    boundary->getCorner(d) - 0.5 * boundary->getWidth(d));
            else
                new_corner.push_back(
                    boundary->getCorner(d) + 0.5 * boundary->getWidth(d));
            div *= 2;
        }
        children[i] = std::make_unique<SPTree>(
            dimension, data, std::move(new_corner), std::move(new_width));
    }

    // Move existing points to correct children
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < no_children; j++)
        {
            if (children[j]->insert(index[i]))
                break;
        }
        index[i] = -1;
    }

    // Empty parent node
    size = 0;
    is_leaf = false;
}

// Build SPTree on dataset
void SPTree::fill(int N)
{
    for (int i = 0; i < N; i++)
        insert(i);
}

// Compute non-edge forces using Barnes-Hut algorithm
void SPTree::computeNonEdgeForces(int point_index, double theta,
    std::vector<double>& neg_f, int neg_offset, double& sum_Q)
{
    // Make sure that we spend no time on empty nodes or self-interactions
    if (cum_size == 0 || (is_leaf && size == 1 && index[0] == point_index))
        return;

    // Compute distance between point and center-of-mass
    double D = 0.0;
    int ind = point_index * dimension;
    for (int d = 0; d < dimension; d++)
    {
        double temp = data[ind + d] - center_of_mass[d];
        D += temp * temp;
    }

    // Check whether we can use this node as a "summary"
    double max_width = boundary->getMaxWidth();
    if (is_leaf || max_width / std::sqrt(D) < theta)
    {
        // Compute and add t-SNE force between point and current node
        D = 1.0 / (1.0 + D);
        double mult = cum_size * D;
        sum_Q += mult;
        mult *= D;
        if (!neg_f.empty())
        {
            for (int d = 0; d < dimension; d++)
                neg_f[d + neg_offset] += mult * (data[ind + d] - center_of_mass[d]);
        }
    }
    else
    {
        // Recursively apply Barnes-Hut to children
        for (int i = 0; i < no_children; i++)
            children[i]->computeNonEdgeForces(
                point_index, theta, neg_f, neg_offset, sum_Q);
    }
}
}

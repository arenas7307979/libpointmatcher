// kate: replace-tabs off; indent-width 4; indent-mode normal
// vim: ts=4:sw=4:noexpandtab
/*

Copyright (c) 2010--2018,
François Pomerleau and Stephane Magnenat, ASL, ETHZ, Switzerland
You can contact the authors at <f dot pomerleau at gmail dot com> and
<stephane at magnenat dot net>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETH-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include "Geometry.h"


// TODO: Rename properly
// Constructor
template<typename T>
GeometryDataPointsFilter<T>::GeometryDataPointsFilter(const Parameters& params):
	PointMatcher<T>::DataPointsFilter("GeometryDataPointsFilter",
			GeometryDataPointsFilter::availableParameters(), params),
			keepUnstructureness(Parametrizable::get<int>("keepUnstructureness")),
			keepStructureness(Parametrizable::get<T>("keepStructureness"))
{
}

// Compute
template<typename T>
typename PointMatcher<T>::DataPoints
GeometryDataPointsFilter<T>::filter(
	const DataPoints& input)
{
	DataPoints output(input);
	inPlaceFilter(output);
	return output;
}

// In-place filter
template<typename T>
void GeometryDataPointsFilter<T>::inPlaceFilter(
	DataPoints& cloud)
{

	typedef typename DataPoints::View View;
	typedef typename DataPoints::Label Label;
	typedef typename DataPoints::Labels Labels;

	const size_t pointsCount(cloud.features.cols());
	const size_t descDim(cloud.descriptors.rows());
	const size_t labelDim(cloud.descriptorLabels.size());

	// Check that the required eigenValue descriptor exists in the pointcloud
	if (!cloud.descriptorExists("eigValues"))
	{
		throw InvalidField("GeometryDataPointsFilter: Error, no eigValues found in descriptors.");
	}

	// Validate descriptors and labels
	size_t insertDim(0);
	for(unsigned int i = 0; i < labelDim ; ++i)
		insertDim += cloud.descriptorLabels[i].span;
	if (insertDim != descDim)
		throw InvalidField("SurfaceNormalDataPointsFilter: Error, descriptor labels do not match descriptor data");

	// Reserve memory for new descriptors
	const size_t unidimensionalDescriptorDimension(1);


	boost::optional<View> sphericality;
	boost::optional<View> unstructureness;
	boost::optional<View> structureness;

	Labels cloudLabels;
	cloudLabels.push_back(Label("sphericality", unidimensionalDescriptorDimension));
	if (keepUnstructureness)
		cloudLabels.push_back(Label("unstructureness", unidimensionalDescriptorDimension));
	if (keepStructureness)
		cloudLabels.push_back(Label("structureness", unidimensionalDescriptorDimension));

	// Reserve memory
	cloud.allocateDescriptors(cloudLabels);

	// Get the views
	const View eigValues = cloud.getDescriptorViewByName("eigValues");
	if (eigValues.rows() != 3)  // And check the dimensions
	{
		throw InvalidField("GeometryDataPointsFilter: Error, the number of eigValues is not 3.");
	}

	sphericality = cloud.getDescriptorViewByName("sphericality");
	if (keepUnstructureness)
		unstructureness = cloud.getDescriptorViewByName("unstructureness");
	if (keepStructureness)
		structureness = cloud.getDescriptorViewByName("structureness");

	// Iterate through the point cloud and evaluate the geometry
	for (size_t i = 0; i < pointsCount; ++i)
	{
		// extract the three eigenvalues relevant to the current point
		Vector eig_vals_col = eigValues.col(i);
		// might be already sorted but sort anyway
		std::sort(eig_vals_col.data(),eig_vals_col.data()+eig_vals_col.size());

		// Finally, evaluate the geometry
		T sphericalityVal;
		T unstructurenessVal;
		T structurenessVal;

		// First, avoid division by zero
		//TODO: Is there a more suitable limit for considering the values almost-zero? (VK)
		if (fabs(eig_vals_col(2)) < std::numeric_limits<T>::min() or
			fabs(eig_vals_col(1)) < std::numeric_limits<T>::min())
		{
			// If either the largest or the middle eigenvalue are zeros, these descriptors are not well defined (0/0)
			sphericalityVal = std::numeric_limits<T>::quiet_NaN();
			unstructurenessVal = std::numeric_limits<T>::quiet_NaN();
			structurenessVal = std::numeric_limits<T>::quiet_NaN();
		} else {
			// Otherwise, follow eq.(1) from Kubelka V. et al., "Radio propagation models for differential GNSS based
			// on dense point clouds", JFR, hopefully published in 2020
			unstructurenessVal = eig_vals_col(0) / eig_vals_col(2);
			structurenessVal =  (eig_vals_col(1) / eig_vals_col(2)) *
				((eig_vals_col(1) - eig_vals_col(0)) / sqrt(eig_vals_col(0)*eig_vals_col(0) + eig_vals_col(1)*eig_vals_col(1)));
			sphericalityVal = unstructurenessVal - structurenessVal;
		}

		// store in the pointcloud
		(sphericality.get())(0,i) = sphericalityVal;
		if (keepUnstructureness)
			(unstructureness.get())(0,i) = unstructurenessVal;
		if (keepStructureness)
			(structureness.get())(0,i) = structurenessVal;

	}
}

template struct GeometryDataPointsFilter<float>;
template struct GeometryDataPointsFilter<double>;
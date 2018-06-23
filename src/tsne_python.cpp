#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <boost/python.hpp>
#include <numpy/arrayobject.h>

#include <iostream>
#include <utility>
#include <vector>

namespace bp = boost::python;

void checkArray(PyArrayObject* arr)
{
	if (!(PyArray_FLAGS(arr) & NPY_ARRAY_C_CONTIGUOUS))
		throw std::runtime_error("Passed array must be contiguous.");
	if (PyArray_NDIM(arr) != 2) //TODO: change to 2D
		throw std::runtime_error("Passed array must be 2D.");
	if (PyArray_TYPE(arr) != NPY_float64)
		throw std::runtime_error("Passed array must be of type double64.");
}

std::vector<npy_intp> getDims(PyArrayObject* arr)
{
	std::vector<npy_intp> dims(2);
	dims[0] = PyArray_DIMS(arr)[0];
	dims[1] = PyArray_DIMS(arr)[1];
	return dims;
}

std::vector<double> npToStdVec(bp::object obj, std::vector<npy_intp>& dims)
{
	auto arrObj = reinterpret_cast<PyArrayObject*>(obj.ptr());
	checkArray(arrObj);
	dims = getDims(arrObj);
	auto arrValuePtr = static_cast<double*>(PyArray_DATA(arrObj));
	return std::vector<double>(arrValuePtr, arrValuePtr + dims[0] * dims[1]);
}

void writeResults(PyObject* obj, const std::vector<double>& res,
	const std::vector<npy_intp>& dims)
{
	auto arrObj = reinterpret_cast<PyArrayObject*>(obj);
	checkArray(arrObj);
	auto arrValuePtr = static_cast<double*>(PyArray_DATA(arrObj));

	for(int i = 0; i < static_cast<int>(res.size()); i++)
		*(arrValuePtr++) = res[i];
}


PyObject* tsne(bp::object obj, int noDims, double perplexity, double theta,
		int maxIt, int stopLyingIt, int momentumSwitch)
{
	//std::cout << noDims << std::endl;
	//std::cout << perplexity << std::endl;
	//std::cout << theta << std::endl;
	//std::cout << maxIt << std::endl;
	//std::cout << stopLyingIt << std::endl;
	//std::cout << momentumSwitch << std::endl;
	std::vector<npy_intp> dims;
	auto vec = npToStdVec(obj, dims);
	for(auto& elem : vec)
		std::cout << elem++ << std::endl;

	//call tsne on vec

	PyObject *outArray = PyArray_SimpleNew(2, dims.data(), NPY_float64);
	writeResults(outArray, vec, dims);

	return outArray;
}

BOOST_PYTHON_MODULE(test)
{
	using namespace boost::python;
	def("tsne", tsne, bp::default_call_policies(), (bp::arg("obj"), bp::arg("noDims"),
				bp::arg("perplexity"), bp::arg("theta"), bp::arg("maxIt")=1000,
				bp::arg("stopLyingIt")=250, bp::arg("momentumSwitch")=250));

	import_array(); // needed for PyArray_SimpleNew
}



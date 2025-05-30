//
// Array.cpp
//
// Library: JSON
// Package: JSON
// Module:  Array
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/JSON/Array.h"
#include "DBPoco/JSON/Object.h"
#include "DBPoco/JSON/Stringifier.h"
#include "DBPoco/JSONString.h"


using DBPoco::Dynamic::Var;


namespace DBPoco {
namespace JSON {


Array::Array(int options): _modified(false),
	_escapeUnicode((options & DBPoco::JSON_ESCAPE_UNICODE) != 0)
{
}


Array::Array(const Array& other) : _values(other._values),
	_pArray(other._pArray),
	_modified(other._modified)
{
}


Array &Array::operator=(const Array& other)
{
	if (&other != this)
	{
		_values = other._values;
		_pArray = other._pArray;
		_modified = other._modified;
	}
	return *this;
}

#ifdef DB_POCO_ENABLE_CPP11


Array::Array(Array&& other) :
	_values(std::move(other._values)),
	_pArray(!other._modified ? other._pArray : 0),
	_modified(other._modified)
{
	_pArray = 0;
}

Array &Array::operator= (Array&& other)
{
	if (&other != this)
	{
		_values = std::move(other._values);
		_pArray = other._pArray;
		other._pArray = 0;
		_modified = other._modified;
	}
	return *this;
}


#endif // DB_POCO_ENABLE_CPP11


Array::~Array()
{
}


Var Array::get(unsigned int index) const
{
	Var value;
	try
	{
		value = _values.at(index);
	}
	catch (std::out_of_range&)
	{
		//Ignore, we return an empty value
	}
	return value;
}


Array::Ptr Array::getArray(unsigned int index) const
{
	Array::Ptr result;

	Var value = get(index);
	if (value.type() == typeid(Array::Ptr))
	{
		result = value.extract<Array::Ptr>();
	}
	return result;
}


Object::Ptr Array::getObject(unsigned int index) const
{
	Object::Ptr result;

	Var value = get(index);
	if (value.type() == typeid(Object::Ptr))
	{
		result = value.extract<Object::Ptr>();
	}
	return result;
}


bool Array::isNull(unsigned int index) const
{
	if (index < _values.size())
	{
		Dynamic::Var value = _values[index];
		return value.isEmpty();
	}
	return true;
}


bool Array::isObject(unsigned int index) const
{
	Var value = get(index);
	return isObject(value);
}


bool Array::isObject(const Dynamic::Var& value) const
{
	return value.type() == typeid(Object::Ptr);
}


bool Array::isObject(ConstIterator& it) const
{
	return it!= end() && isObject(*it);
}


void Array::stringify(std::ostream& out, unsigned int indent, int step) const
{
	int options = DBPoco::JSON_WRAP_STRINGS;
	options |= _escapeUnicode ? DBPoco::JSON_ESCAPE_UNICODE : 0;

	if (step == -1) step = indent;

	out << "[";

	if (indent > 0) out << std::endl;

	for (ValueVec::const_iterator it = _values.begin(); it != _values.end();)
	{
		for (int i = 0; i < indent; i++) out << ' ';

		Stringifier::stringify(*it, out, indent + step, step, options);

		if (++it != _values.end())
		{
			out << ",";
			if (step > 0) out << '\n';
		}
	}

	if (step > 0) out << '\n';

	if (indent >= step) indent -= step;

	for (int i = 0; i < indent; i++) out << ' ';

	out << "]";
}


void Array::resetDynArray() const
{
	if (!_pArray)
		_pArray = new DBPoco::Dynamic::Array;
	else
		_pArray->clear();
}


Array::operator const DBPoco::Dynamic::Array& () const
{
	if (!_values.size())
	{
		resetDynArray();
	}
	else if (_modified)
	{
		ValueVec::const_iterator it = _values.begin();
		ValueVec::const_iterator end = _values.end();
		resetDynArray();
		int index = 0;
		for (; it != end; ++it, ++index)
		{
			if (isObject(it))
			{
				_pArray->insert(_pArray->end(), DBPoco::JSON::Object::makeStruct(getObject(index)));
			}
			else if (isArray(it))
			{
				_pArray->insert(_pArray->end(), makeArray(getArray(index)));
			}
			else
			{
				_pArray->insert(_pArray->end(), *it);
			}
		}
		_modified = false;
	}

	return *_pArray;
}


DBPoco::Dynamic::Array Array::makeArray(const JSON::Array::Ptr& arr)
{
	DBPoco::Dynamic::Array vec;

	JSON::Array::ConstIterator it  = arr->begin();
	JSON::Array::ConstIterator end = arr->end();
	int index = 0;
	for (; it != end; ++it, ++index)
	{
		if (arr->isObject(it))
		{
			Object::Ptr pObj = arr->getObject(index);
			DynamicStruct str = DBPoco::JSON::Object::makeStruct(pObj);
			vec.insert(vec.end(), str);
		}
		else if (arr->isArray(it))
		{
			Array::Ptr pArr = arr->getArray(index);
			std::vector<DBPoco::Dynamic::Var> v = makeArray(pArr);
			vec.insert(vec.end(), v);
		}
		else
			vec.insert(vec.end(), *it);
	}

	return vec;
}


void Array::clear()
{
	_values.clear();
	_pArray = 0;
}


} } // namespace DBPoco::JSON

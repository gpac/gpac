/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de> 
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>
 */

#ifndef SINGLETON_H_
#define SINGLETON_H_



namespace avcap 
{
	//! Base for classes that implement the singleton pattern. 
	
	/*! There are lots of more spohisticated approaches to implement 
	 * a singleton but this simple approach is sufficient here.  */
	template <class T>
	class Singleton
	{
	public:
	  
		//! Get the global instance of the singleton.
		static T& instance() 
		{
			static T _instance;
			return _instance;
		}
	  
	private:
	
		Singleton();	// ctor hidden
		~Singleton();	// dtor hidden
		Singleton(Singleton const&);	// copy ctor hidden
		Singleton& operator=(Singleton const&);	// assign op hidden
	};
}

#endif // SINGLETON_H_


#pragma once
#include<string>
#include<vector>
#include<memory>
#include<algorithm>
#include<functional>
#include<array>
#include<regex>

template<class T,class ITERATOR,class RESULT_TYPE>
class basic_puneuma{
public:
	typedef ITERATOR Iterator;
	typedef RESULT_TYPE result_type;
	struct puneuma_error:public std::runtime_error
	{
		puneuma_error(const std::string&str)
			:std::runtime_error(str)
		{}
	};
protected:
	class Ast;
	typedef std::function<result_type(const Ast&)> Call_Func;
	typedef std::shared_ptr<Call_Func> CallFuncPtr;
	class Ast
	{
	public:
		Ast(const CallFuncPtr&func)
			:func(func)
		{}
		virtual const T&Get(size_t)const=0;
		virtual size_t size()const=0;
		virtual const result_type Child(size_t)const=0;
		virtual size_t ChildSize()const=0;
		virtual ~Ast(){}
	
		result_type operator()()const
		{
			if(func)
				return (*func)(*this);
			if(ChildSize()==1)
				return Child(0);
			throw std::runtime_error("InvalidFunc");
		}
	private:
			CallFuncPtr func;
	};
	typedef Ast Arg;
	typedef std::shared_ptr<Ast> ast_ptr;
private:
	class Has_Child
	{
	protected:
		template<class Iter>
		Has_Child(const Iter&begin,const Iter&end)
			:data(begin,end)
		{}
		size_t _child_size()const{return data.size();}
		const Ast& _child_at(size_t n)const{return *(data.at(n));}
	private:
		std::vector<ast_ptr> data;
	};
	class Has_stack
	{
	protected:
		template<class Iter>
		Has_stack(const Iter&begin,const Iter&end)
			:data(begin,end)
		{}
		const T& _get_stack(size_t n)const{return data.at(n);}
		size_t _size_stack()const{return data.size();}
	private:
		std::vector<T> data;
	};
	class AstPolicy:public Ast,Has_Child,Has_stack
	{
	public:
		template<class Data,class Asts>
		AstPolicy(Data&data,Asts&ast,const CallFuncPtr&func)
			:Has_Child(ast.begin(),ast.end())
			,Has_stack(data.begin(),data.end())
			,Ast(func)
		{}
		const T&Get(size_t n)const{return _get_stack(n);}
		size_t size()const{return _size_stack();}
		const result_type Child(size_t n)const{return _child_at(n)();}
		size_t ChildSize()const{return _child_size();}
	};
protected:

	template<class Data,class Ast>
	static ast_ptr MakeAst(Data&data,Ast&ast,const CallFuncPtr&func)
	{
		return ast_ptr(new AstPolicy(data,ast,func));
	}
	//デバック情報など一つ必要なデータ
	//holder(=構文木)が必要とするデータ
	struct holder_data
	{
		std::vector<T> st;
		std::vector<ast_ptr> ast;
		CallFuncPtr func;
		const holder_data&operator+=(const holder_data&rhs)
		{
			if(rhs.func)
				func=rhs.func;
			st.insert(st.end(),rhs.st.begin(),rhs.st.end());
			ast.insert(ast.end(),rhs.ast.begin(),rhs.ast.end());
			return *this;
		}
	};


	
	struct result_data
	{
		const result_data& operator+=(const result_data&rhs)
		{
			return *this;
		}

	};
	class Factor
	{
	public:
		virtual bool operator()(Iterator &it,const Iterator&end,holder_data&,result_data&)const=0;
	};
	typedef std::shared_ptr<Factor> Factor_Ptr;
	class Holder;
	

	class Facter_expr
	{
	public:
		Facter_expr(const Factor_Ptr&pt)
		{
			data.push_back(pt);
		}
		Facter_expr(std::vector<Factor_Ptr>&&original)
			:data(std::move(original))
		{}
		
		Facter_expr operator&(const Facter_expr&rhs)const
		{
			std::vector<Factor_Ptr> temp;
			temp.reserve(data.size()+rhs.data.size());
			temp=data;
			temp.insert(temp.end(),rhs.data.begin(),rhs.data.end());
			return std::move(temp);
		}
		inline Facter_expr operator |(const Facter_expr&rhs)const
		{
			class Or:public Factor
			{
			public:
				Or(const Facter_expr&lhs,const Facter_expr&rhs)
					:lhs(lhs),rhs(rhs)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
				{
					holder_data h;
					result_data r;
					Iterator i=it;
					if((*lhs)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
						return true;
					}
					return (*rhs)(it,end,holder,result);
				}
			private:
				Factor_Ptr lhs,rhs;
			};
			return Factor_Ptr (new Or(*this,rhs));  
		}
		inline Facter_expr operator *()const
		{
			class Loop:public Factor
			{
			public:
				Loop(const Facter_expr&rhs)
					:rhs(rhs)
				{}

				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
				{
					holder_data h;
					result_data r;
					Iterator i=it;
					if((*rhs)(i,end,h,r) && (*this)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
					}
					return true;
				}
			private:
				Factor_Ptr rhs;
			};
			return Factor_Ptr (new Loop(*this));  
		}
		inline Facter_expr operator +()const
		{
			return *this&*(*this);
		}
		inline Facter_expr operator!()const
		{
			class Option:public Factor
			{
			public:
				Option(const Facter_expr&rhs)
					:rhs(rhs)
				{}

				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
				{
					holder_data h;
					result_data r;
					Iterator i=it;
					if((*rhs)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
					}
					return true;
				}
			private:
				Factor_Ptr rhs;
			};
			return Factor_Ptr (new Option(*this));  

		}
		operator Factor_Ptr()const
		{
			class wrapper_Facter_expr:public Factor
			{
			public:
				wrapper_Facter_expr(const std::vector<Factor_Ptr>&data)
					:data(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
				{
					return 
						std::all_of(
						data.begin(),data.end(),[&](const Factor_Ptr&factor){
							return (*factor)(it,end,holder,result);
					});
				}
			private:
				std::vector<Factor_Ptr> data;
			};
			return Factor_Ptr (new wrapper_Facter_expr(data)); 
		}
		Factor_Ptr Wrap()const
		{
			return *this;
		}
		const std::vector<Factor_Ptr>&Get()const
		{
			return data;
		}
	private:
		std::vector<Factor_Ptr> data;
	};
	typedef std::vector<Factor_Ptr> HolderData;
	class Holder
	{
	public:
		Holder(HolderData&data)
			:data(data)
		{}
		inline void operator=(const Facter_expr&rhs)
		{
			data=rhs.Get();
		}
		inline void operator=(const Holder&rhs)
		{
			data=static_cast<Facter_expr>(rhs).Get();
		}
		bool operator()(Iterator &it,const Iterator&end,holder_data& hold,result_data&result)const
		{
			holder_data myhold;
			if(std::all_of(data.begin(),data.end(),[&](const Factor_Ptr&factor){return (*factor)(it,end,myhold,result);}))
			{
				if(myhold.st.size()==0 && !myhold.func && myhold.ast.size()==1)
				{
					hold.ast.push_back(myhold.ast.at(0));
				}
				else
				{
					hold.ast.push_back(MakeAst(myhold.st,myhold.ast,myhold.func));
				}
				return true;
			}
			return false;
		}
		operator Facter_expr()const
		{
			class ref_Holder:public Factor
			{
			public:
				ref_Holder(HolderData &data)
					:holder(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&hold,result_data&result)const
				{
					return holder(it,end,hold,result);
				}
			private:
				Holder holder;
			};
			return Factor_Ptr (new ref_Holder(data));
		}
		inline Facter_expr operator&(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)&rhs;
		}
		inline Facter_expr operator|(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)|rhs;
		}
		inline Facter_expr operator*()const
		{
			return *static_cast<Facter_expr>(*this);
		}
		inline Facter_expr operator!()const
		{
			return !static_cast<Facter_expr>(*this);
		}
		inline HolderData&GetHolderData()
		{
			return data;
		}
	private:
		HolderData &data;
	};

	class Parser
	{
	public:
		result_type operator()(Iterator&it,const Iterator&end)const
		{
			Holder holder(*start);
			result_data r;
			holder_data h;
			if(holder(it,end,h,r))
			{
				return (*h.ast.at(0))();
			}
			throw std::runtime_error("faild_syntax");
		}
		void operator=(const Facter_expr&rhs)
		{
			start=std::make_shared<HolderData>();
			Holder h(*start);
			h=rhs;
		}
		Holder MakeHolder()
		{
			auto p=std::make_shared<HolderData>();
			data.push_back(p);
			return Holder(*p);
		}
	private:
		typedef std::shared_ptr<HolderData> HolderDataPtr;
		std::vector<HolderDataPtr> data;
		HolderDataPtr start;
	};
	
	
	
	inline static Facter_expr Set(const Call_Func&func)
	{
		class SetCallFunc:public Factor
		{
		public:
			SetCallFunc(const Call_Func&f)
				:fun(std::make_shared<Call_Func>(f))
			{}
			bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
			{
				holder.func=fun;
				return true;
			}
		private:
			CallFuncPtr fun;
		};
		return Factor_Ptr(new SetCallFunc(func));
	}
	
	template<class FUNC,bool PUSH>
	class basic_symbol:public Factor
	{
	public:
		basic_symbol(const FUNC&f)
			:f(f)
		{}
		bool operator()(Iterator &it,const Iterator&end,holder_data&holder,result_data&result)const
		{
			if(it!=end && f(*it))
			{
				if(PUSH)
					holder.st.push_back(*it);
				++it;
				return true;
			}
			return false;
		}
	private:
		FUNC f;
	}; 
	
	static Facter_expr S(const T&code)
	{
		class Match
		{
		public:
			Match(const T&x)
				:data(x)
			{}
			bool operator()(const T&x)const
			{
				return x==data;
			}
		private:
			T data;
		};
		typedef basic_symbol<Match,false> sym;
		return Factor_Ptr(new sym(Match(code)));
	}
	static Facter_expr SP(const T&code)
	{
		class Match
		{
		public:
			Match(const T&x)
				:data(x)
			{}
			bool operator()(const T&x)const
			{
				return x==data;
			}
		private:
			T data;
		};
		typedef basic_symbol<Match,true> sym;
		return Factor_Ptr(new sym(Match(code)));
	}
};

template<class Iterator,class result_type>
class puneuma:public basic_puneuma<std::string,Iterator,result_type>
{
public:
	static Facter_expr RT(const std::string&code)
	{
		class Reg
		{
		public:
			Reg(const std::string&regex)
				:re(regex)
			{}
			bool operator()(const std::string&x)const
			{
				return std::regex_match(x,re);
			}
		private:
			std::regex re;
		};
		typedef basic_symbol<Reg,true> sym;
		return Factor_Ptr(new sym(Reg(code)));
	}
};
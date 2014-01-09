#ifndef MethodMock_h__
#define MethodMock_h__

#include <vector>
#include <functional>

#include "mockutils/TupleDispatcher.h"
#include "mock4cpp/InvocationMatcher.h"
#include "mock4cpp/MockRepository.h"
#include "mock4cpp/ActualInvocation.h"
#include "mock4cpp/Exceptions.h"

namespace mock4cpp {

template<typename R, typename ... arglist>
struct BehaviorMock {
	virtual R invoke(const arglist&... args) = 0;
};

template<typename R, typename ... arglist>
struct DoMock: public BehaviorMock<R, arglist...> {
	DoMock(std::function<R(arglist...)> f) :
			f(f) {
	}
	virtual R invoke(const arglist&... args) override {
		return f(args...);
	}
private:
	std::function<R(arglist...)> f;
};

template<typename R, typename ... arglist>
struct MethodInvocationMock: public InvocationMatcher<arglist...>, public MethodInvocationHandler<R, arglist...> {

};

template<typename R, typename ... arglist>
struct MethodBehaviorRecorder: public MethodInvocationHandler<R, arglist...>  {

	void append(std::shared_ptr<BehaviorMock<R, arglist...>> mock) {
		behaviorMocks.push_back(mock);
	}

	void appendDo(std::function<R(arglist...)> method) {
		auto doMock = std::shared_ptr<BehaviorMock<R, arglist...>> { new DoMock<R, arglist...>(method) };
		append(doMock);
	}

	void clear() {
		behaviorMocks.clear();
	}

	R handleMethodInvocation(const arglist&... args) override {
		std::shared_ptr<BehaviorMock<R, arglist...>> behavior = behaviorMocks.front();
		if (behaviorMocks.size() > 1)
			behaviorMocks.erase(behaviorMocks.begin());
		return behavior->invoke(args...);
	}

private:
	std::vector<std::shared_ptr<BehaviorMock<R, arglist...>>>behaviorMocks;
};

template<typename R, typename ... arglist>
struct MethodInvocationMockBase: public virtual MethodInvocationMock<R, arglist...> {

	MethodInvocationMockBase(std::shared_ptr<InvocationMatcher<arglist...>> matcher, std::function<R(arglist...)> methodBehavior) :
			matcher { matcher } {
				recorder.appendDo(methodBehavior);
	}

	void append(std::shared_ptr<BehaviorMock<R, arglist...>> mock) {
		recorder.append(mock);
	}

	void appendDo(std::function<R(arglist...)> method) {
		recorder.appendDo(method);
	}

	void clear() {
		recorder.clear();
	}

	R handleMethodInvocation(const arglist&... args) override {
		return recorder.handleMethodInvocation(args...);
	}

	virtual bool matches(ActualInvocation<arglist...>& actualArgs) {
		return matcher->matches(actualArgs);
	}

private:
	std::shared_ptr<InvocationMatcher<arglist...>> matcher;
	MethodBehaviorRecorder<R, arglist...> recorder;
};

template<typename ... arglist>
struct ExpectedArgumentsInvocationMatcher: public InvocationMatcher<arglist...> {
	ExpectedArgumentsInvocationMatcher(const arglist&... args) :
			expectedArguments(args...) {
	}

	virtual bool matches(ActualInvocation<arglist...>& invocation) override {
		return matches(invocation.getActualArguments());
	}
private:
	virtual bool matches(const std::tuple<arglist...>& actualArgs) {
		return expectedArguments == actualArgs;
	}
	const std::tuple<arglist...> expectedArguments;
};

template<typename ... arglist>
struct UserDefinedInvocationMatcher: public InvocationMatcher<arglist...> {
	UserDefinedInvocationMatcher(std::function<bool(arglist...)> matcher) :
			matcher { matcher } {
	}

	virtual bool matches(ActualInvocation<arglist...>& invocation) override {
		return matches(invocation.getActualArguments());
	}
private:
	virtual bool matches(const std::tuple<arglist...>& actualArgs) {
		return invoke<arglist...>(matcher, std::tuple<arglist...> { actualArgs });
	}
	std::function<bool(arglist...)> matcher;
};

template<typename ... arglist>
struct DefaultInvocationMatcher: public InvocationMatcher<arglist...> {
	DefaultInvocationMatcher() {
	}

	virtual bool matches(ActualInvocation<arglist...>& invocation) override {
		return matches(invocation.getActualArguments());
	}

private:
	virtual bool matches(const std::tuple<arglist...>& actualArgs) {
		return true;
	}
};

template<typename R, typename ... arglist>
struct MethodMock: public MethodInvocationHandler<R, arglist...> {
	MethodMock(MockBase& mock) :
			mock(mock) {
	}

	virtual ~MethodMock() override {
	}

	void stubMethodInvocation(std::shared_ptr<MethodInvocationMock<R, arglist...>> methodInvocationMock) {
		methodInvocationMocks.push_back(methodInvocationMock);
	}

	void clear() {
		methodInvocationMocks.clear();
	}

	R handleMethodInvocation(const arglist&... args) override {
		auto actualInvoaction = std::shared_ptr<ActualInvocation<arglist...>> { new ActualInvocation<arglist...>(args...) };
		actualInvocations.push_back(actualInvoaction);
		auto methodInvocationMock = getMethodInvocationMockForActualArgs(*actualInvoaction);
		if (!methodInvocationMock) {
			throw UnmockedMethodCallException();
		}
		return methodInvocationMock->handleMethodInvocation(args...);
	}

	std::vector<std::shared_ptr<ActualInvocation<arglist...>> > getActualInvocations(InvocationMatcher<arglist...>& matcher) {
		std::vector < std::shared_ptr<ActualInvocation<arglist...>> > result;
		for (auto actualInvocation : actualInvocations) {
			if (matcher.matches(*actualInvocation)) {
				result.push_back(actualInvocation);
			}
		}
		return result;
	}

private:

	MockBase& mock;
	std::vector<std::shared_ptr<MethodInvocationMock<R, arglist...>>>methodInvocationMocks;
	std::vector<std::shared_ptr<ActualInvocation<arglist...>>> actualInvocations;

	std::shared_ptr<MethodInvocationMock<R, arglist...>> getMethodInvocationMockForActualArgs(ActualInvocation<arglist...>& invocation) {
		for (auto i = methodInvocationMocks.rbegin(); i != methodInvocationMocks.rend(); ++i) {
			if ((*i)->matches(invocation)) {
				return (*i);
			}
		}
		return nullptr;
	}

};

}
#endif // MethodMock_h__

// Checks QcwServiceRegistry: registration by interface id, first-registration-wins, refusal of
// an object that does not implement the interface, and ownership transfer — an accepted service
// is detached from its parent and stays registered for the rest of the run.

#include "QcwServiceRegistry.h"

#include <QCoreApplication>
#include <QString>
#include <cstdio>

class RegistryTestService
{
public:
    virtual ~RegistryTestService() = default;
    virtual int marker() const = 0;
};

Q_DECLARE_INTERFACE(RegistryTestService, "org.qcw.test.ServiceRegistryTestService")

namespace
{

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

const QString serviceId = QStringLiteral("org.qcw.test.ServiceRegistryTestService");
const QString extraId = QStringLiteral("org.qcw.test.ExtraService");

} // namespace

class RegistryTestServiceImpl : public QObject, public RegistryTestService
{
    Q_OBJECT
    Q_INTERFACES(RegistryTestService)
public:
    explicit RegistryTestServiceImpl(int marker, QObject* parent = nullptr) :
        QObject { parent }, mMarker { marker }
    {
    }

    int marker() const override
    {
        return mMarker;
    }

private:
    int mMarker;
};

class RegistryTestPlainObject : public QObject
{
    Q_OBJECT
};

int main(int argc, char** argv)
{
    QCoreApplication app { argc, argv };

    auto* registry = QcwServiceRegistry::instance();
    check(registry != nullptr, "instance() is never null");
    check(!registry->service<RegistryTestService>(), "no service before registration");

    // The registry owns every accepted service, so these last until the process exits.
    auto* first = new RegistryTestServiceImpl { 1 };
    check(registry->registerService<RegistryTestService>(first), "first registration succeeds");
    check(registry->service<RegistryTestService>() == first, "lookup returns the registered object");
    check(registry->service<RegistryTestService>()->marker() == 1, "lookup casts to the interface");
    check(registry->contains(serviceId), "contains() reports the interface");
    check(registry->interfaceIds().contains(serviceId), "interfaceIds() lists the interface");
    check(registry->size() >= 1, "size() counts the service");
    check(!registry->empty(), "empty() is false with a service registered");

    auto* second = new RegistryTestServiceImpl { 2 };
    check(!registry->registerService<RegistryTestService>(second), "a second registration is refused");
    check(registry->service<RegistryTestService>() == first, "the first service keeps answering");
    delete second; // a refused service stays with the caller

    RegistryTestPlainObject plain;
    check(!registry->registerService<RegistryTestService>(&plain),
          "an object that does not implement the interface is refused");
    check(!registry->registerService<RegistryTestService>(nullptr), "a null service is refused");
    check(!registry->registerObject(QString(), first), "an empty interface id is refused");

    auto* formerParent = new QObject;
    auto* adopted = new RegistryTestServiceImpl { 3, formerParent };
    check(registry->registerObject(extraId, adopted), "registerObject() accepts an explicit id");
    check(adopted->parent() == nullptr, "registration detaches the service from its parent");
    delete formerParent;
    check(registry->serviceObject(extraId) == adopted, "a registered service outlives its former parent");

    auto* duplicate = new RegistryTestServiceImpl { 4 };
    check(!registry->registerObject(extraId, duplicate), "a second service for one id is refused");
    check(registry->serviceObject(extraId) == adopted, "the first service keeps the id");
    delete duplicate;

    const QStringList ids = registry->interfaceIds();
    check(ids.contains(serviceId) && ids.contains(extraId), "interfaceIds() lists every service");
    check(registry->size() == ids.size(), "size() matches interfaceIds()");

    // The registry keeps a service for the whole run; the test releases its own after the
    // last lookup so the process exits without leaks.
    delete adopted;
    delete first;

    std::fprintf(stderr, "service_registry_test=%d\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}

#include "service_registry_test.moc"

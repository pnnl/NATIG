#ifndef HELICS_APPLICATION_H
#define HELICS_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <string>

#include "helics-id-tag.h"
#include "helics/helics.hpp"
#include "helics/application_api/MessageOperators.hpp"

namespace ns3 {

class Socket;
class Packet;
class InetSocketAddress;
class Inet6SocketAddress;

/**
 * \ingroup helicsapplication
 * \brief A Helics Application
 *
 * Every packet sent should be returned by the server and received here.
 */
class HelicsApplication : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  static std::string& SanitizeName (std::string &name);
  static std::string SanitizeName (const std::string &name);

  HelicsApplication ();

  virtual ~HelicsApplication ();

  /**
   * \brief set the name
   * \param name name
   */
  void SetName (const std::string &name);
  /**
   * \brief set the named endpoint to filter
   * \param name name
   */
  void SetFilterName (const std::string &name);

  /**
   * \brief add filter
   * @param name name
   */
  void AddFilter (const std::string &name);

  /**
   * \brief create the filter application with an existing filter and endpoint
   * \param filterInstance the instance of the helics filter object
   * \param epInstance the instance of the helics endpoint object
   */
  void SetupFilterApplication (const helics::Filter &filterInstance, const helics::Endpoint &epInstance);
  /**
   * \brief set the named of this endpoint
   * \param name name
   */
  void SetEndpointName (const std::string &name, bool is_global);

  void SetEndpoint (helics::Endpoint &ep);
  /**
   * \brief set the local address and port
   * \param ip local IPv4 address
   * \param port local port
   */
  void SetLocal (Ipv4Address ip, uint16_t port);
  /**
   * \brief set the local address and port
   * \param ip local IPv6 address
   * \param port local port
   */
  void SetLocal (Ipv6Address ip, uint16_t port);
  /**
   * \brief set the local address and port
   * \param ip local IP address
   * \param port local port
   */
  void SetLocal (Address ip, uint16_t port);

  std::string GetName (void) const;

  InetSocketAddress GetLocalInet (void) const;

  Inet6SocketAddress GetLocalInet6 (void) const;

  /**
   * \brief Packet creation based on HELICS data sent to dest.
   */
  void Send (std::string dest, helics::Time time, std::unique_ptr<helics::Message> message);

  /**
   * \brief Receive a HELICS message.
   *
   * This function is called internally by HELICS.
   */
  void EndpointCallback (helics::Endpoint id, helics::Time time);

protected:
  virtual void DoDispose (void);
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  virtual void DoFilter (std::unique_ptr<helics::Message> message);
  virtual void DoEndpoint (helics::Endpoint id, helics::Time time);
  virtual void DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message);
  virtual void DoRead (std::unique_ptr<helics::Message> message);

  helics::Endpoint m_endpoint_id;

private:
  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  /**
   * Create a new HelicsIdTag.
   */
  HelicsIdTag NewTag ();

  std::string m_name; //!< name of this application
  uint32_t m_sent; //!< Counter for sent packets
  Ptr<Socket> m_socket; //!< Socket
  Address m_localAddress; //!< Local address
  uint16_t m_localPort; //!< Local port
  Ptr<UniformRandomVariable> m_rand_delay_ns; //!<random value used to add jitter to packet transmission
  double m_jitterMinNs; //!<minimum jitter delay time for packets sent via HELICS
  double m_jitterMaxNs; //!<maximum jitter delay time for packets sent via HELICS
  std::string f_name; //!< name of the output file

  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;

  uint32_t m_next_tag_id;
  helics::Filter m_filter_id;
  std::map<uint32_t,std::unique_ptr<helics::Message> > m_messages;

  std::shared_ptr<helics::MessageDestOperator> m_filterOp; //!< the filter operator for this application
};

} // namespace ns3

#endif /* HELICS_APPLICATION_H */

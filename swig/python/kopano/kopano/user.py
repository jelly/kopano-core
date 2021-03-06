"""
Part of the high-level python bindings for Kopano

Copyright 2005 - 2016 Zarafa and its licensors (see LICENSE file for details)
Copyright 2016 - Kopano and its licensors (see LICENSE file for details)
"""

from .store import Store
from .group import Group
from .quota import Quota

from .errors import *
from .defs import *

from MAPI.Util import *

from .compat import unhex as _unhex, repr as _repr, fake_unicode as _unicode
from .utils import prop as _prop, props as _props

class User(object):
    """User class"""

    def __init__(self, name=None, server=None, email=None):
        from .server import Server

        server = server or Server()
        self.server = server

        if email:
            try:
                self._name = _unicode(server.gab.ResolveNames([PR_EMAIL_ADDRESS_W], MAPI_UNICODE | EMS_AB_ADDRESS_LOOKUP, [[SPropValue(PR_DISPLAY_NAME_W, _unicode(email))]], [MAPI_UNRESOLVED])[0][0][1].Value)
            except (MAPIErrorNotFound, MAPIErrorInvalidParameter, IndexError):
                raise NotFoundError("no such user '%s'" % email)
        else:
            self._name = _unicode(name)

        try:
            self._ecuser = self.server.sa.GetUser(self.server.sa.ResolveUserName(self._name, MAPI_UNICODE), MAPI_UNICODE)
        except (MAPIErrorNotFound, MAPIErrorInvalidParameter): # multi-tenant, but no '@' in username..
            raise NotFoundError("no such user: '%s'" % self.name)
        self._mapiobj = None

    @property
    def mapiobj(self):
        if not self._mapiobj:
            self._mapiobj = self.server.mapisession.OpenEntry(self._ecuser.UserID, None, 0)
        return self._mapiobj

    @property
    def admin(self):
        return self._ecuser.IsAdmin >= 1

    @admin.setter
    def admin(self, value):
        self._update(admin=value)

    @property
    def admin_level(self):
        return self._ecuser.IsAdmin

    @admin_level.setter
    def admin_level(self, value):
        self._update(admin=value)

    @property
    def hidden(self):
        return self._ecuser.IsHidden == True

    @property
    def name(self):
        """ Account name """

        return self._name

    @name.setter
    def name(self, value):
        self._update(username=_unicode(value))

    @property
    def fullname(self):
        """ Full name """

        return self._ecuser.FullName

    @fullname.setter
    def fullname(self, value):
        self._update(fullname=_unicode(value))

    @property
    def email(self):
        """ Email address """

        return self._ecuser.Email

    @email.setter
    def email(self, value):
        self._update(email=_unicode(value))

    @property
    def password(self): # XXX not coming through SWIG?
        return self._ecuser.Password

    @password.setter
    def password(self, value):
        self._update(password=_unicode(value))

    @property
    def features(self):
        """ Enabled features (pop3/imap/mobile) """

        if not hasattr(self._ecuser, 'MVPropMap'):
            raise NotSupportedError('Python-Mapi does not support MVPropMap')

        for entry in self._ecuser.MVPropMap:
            if entry.ulPropId == PR_EC_ENABLED_FEATURES_W:
                return entry.Values

    @features.setter
    def features(self, value):

        if not hasattr(self._ecuser, 'MVPropMap'):
            raise NotSupportedError('Python-Mapi does not support MVPropMap')

        # Enabled + Disabled defines all features.
        features = set([e for entry in self._ecuser.MVPropMap for e in entry.Values])
        disabled = list(features - set(value))

        # XXX: performance
        for entry in self._ecuser.MVPropMap:
            if entry.ulPropId == PR_EC_ENABLED_FEATURES_W:
                entry.Values = value
            if entry.ulPropId == PR_EC_DISABLED_FEATURES_W:
                entry.Values = disabled

        self._update()

    def add_feature(self, feature):
        """ Add a feature for a user

        :param feature: the new feature
        """

        feature = _unicode(feature)
        if feature in self.features:
            raise DuplicateError("feature '%s' already enabled for user '%s'" % (feature, self.name))
        self.features = self.features + [feature]

    def remove_feature(self, feature):
        """ Remove a feature for a user

        :param feature: the to be removed feature
        """

        # Copy features otherwise we will miss a disabled feature.
        # XXX: improvement?
        features = self.features[:]
        try:
            features.remove(_unicode(feature))
        except ValueError:
            raise NotFoundError("no feature '%s' enabled for user '%s'" % (feature, self.name))
        self.features = features
        self._update()

    @property
    def userid(self):
        """ Userid """

        return bin2hex(self._ecuser.UserID)

    @property
    def company(self):
        """ :class:`Company` the user belongs to """

        from .company import Company

        try:
            return Company(HrGetOneProp(self.mapiobj, PR_EC_COMPANY_NAME_W).Value, self.server)
        except MAPIErrorNoSupport:
            return Company(u'Default', self.server)

    @property # XXX
    def local(self):
        store = self.store
        return bool(store and (self.server.guid == bin2hex(HrGetOneProp(store.mapiobj, PR_MAPPING_SIGNATURE).Value)))

    def create_store(self):
        try:
            storeid_rootid = self.server.sa.CreateStore(ECSTORE_TYPE_PRIVATE, self._ecuser.UserID)
        except MAPIErrorCollision:
            raise DuplicateError("user '%s' already has store" % self.name)
        store_entryid = WrapStoreEntryID(0, b'zarafa6client.dll', storeid_rootid[0][:-4])+self.server.pseudo_url+b'\x00'
        return Store(entryid=store_entryid, server = self.server)

    @property
    def store(self):
        """ Default :class:`Store` for user or *None* if no store is attached """

        try:
            entryid = self.server.ems.CreateStoreEntryID(None, self._name, MAPI_UNICODE)
            return Store(entryid=entryid, server=self.server)
        except MAPIErrorNotFound:
            pass

    # XXX deprecated? user.store = .., user.archive_store = ..
    def hook(self, store):
        try:
            self.server.sa.HookStore(ECSTORE_TYPE_PRIVATE, _unhex(self.userid), _unhex(store.guid))
        except MAPIErrorCollision:
            raise DuplicateError("user '%s' already has hooked store" % self.name)

    # XXX deprecated? user.store = None
    def unhook(self):
        try:
            self.server.sa.UnhookStore(ECSTORE_TYPE_PRIVATE, _unhex(self.userid))
        except MAPIErrorNotFound:
            raise NotFoundError("user '%s' has no hooked store" % self.name)

    @property
    def active(self):
        return self._ecuser.Class == ACTIVE_USER

    @active.setter
    def active(self, value):
        if value:
            self._update(user_class=ACTIVE_USER)
        else:
            self._update(user_class=NONACTIVE_USER)

    @property
    def home_server(self):
        return self._ecuser.Servername

    @property
    def archive_server(self):
        try:
            return HrGetOneProp(self.mapiobj, PR_EC_ARCHIVE_SERVERS).Value[0]
        except MAPIErrorNotFound:
            return

    def prop(self, proptag):
        return _prop(self, self.mapiobj, proptag)

    def props(self):
        return _props(self.mapiobj)

    @property
    def quota(self):
        """ User :class:`Quota` """

        return Quota(self.server, self._ecuser.UserID)

    def groups(self):
        for g in self.server.sa.GetGroupListOfUser(self._ecuser.UserID, MAPI_UNICODE):
            yield Group(g.Groupname, self.server)

    def send_as(self):
        for u in self.server.sa.GetSendAsList(self._ecuser.UserID, MAPI_UNICODE):
            yield self.server.user(u.Username)

    def add_send_as(self, user):
        try:
            self.server.sa.AddSendAsUser(self._ecuser.UserID, user._ecuser.UserID)
        except MAPIErrorCollision:
            raise DuplicateError("user '%s' already in send-as for user '%s'" % (user.name, self.name))

    def remove_send_as(self, user):
        try:
            self.server.sa.DelSendAsUser(self._ecuser.UserID, user._ecuser.UserID)
        except MAPIErrorNotFound:
            raise NotFoundError("no user '%s' in send-as for user '%s'" % (user.name, self.name))

    def rules(self):
        return self.inbox.rules()

    def __eq__(self, u): # XXX check same server?
        if isinstance(u, User):
            return self.userid == u.userid
        return False

    def __ne__(self, u):
        return not self == u

    def __unicode__(self):
        return u"User('%s')" % self._name

    def __repr__(self):
        return _repr(self)

    def _update(self, **kwargs):
        username = kwargs.get('username', self.name)
        password = kwargs.get('password', self._ecuser.Password)
        email = kwargs.get('email', _unicode(self._ecuser.Email))
        fullname = kwargs.get('fullname', _unicode(self._ecuser.FullName))
        user_class = kwargs.get('user_class', self._ecuser.Class)
        admin = kwargs.get('admin', self._ecuser.IsAdmin)

        try:
            # Pass the MVPropMAP otherwise the set values are reset
            if hasattr(self._ecuser, 'MVPropMap'):
                usereid = self.server.sa.SetUser(ECUSER(Username=username, Password=password, Email=email, FullName=fullname,
                                             Class=user_class, UserID=self._ecuser.UserID, IsAdmin=admin, MVPropMap = self._ecuser.MVPropMap), MAPI_UNICODE)

            else:
                usereid = self.server.sa.SetUser(ECUSER(Username=username, Password=password, Email=email, FullName=fullname,
                                             Class=user_class, UserID=self._ecuser.UserID, IsAdmin=admin), MAPI_UNICODE)
        except MAPIErrorNoSupport:
            pass

        self._ecuser = self.server.sa.GetUser(self.server.sa.ResolveUserName(username, MAPI_UNICODE), MAPI_UNICODE)
        self._name = username

    def __getattr__(self, x): # XXX add __setattr__, e.g. for 'user.archive_store = None'
        store = self.store
        if store:
            return getattr(store, x)
